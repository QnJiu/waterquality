#define main original_download_test_main
#include "test_download.c"
#undef main
#include "tp_staged_copy.h"

typedef struct { fake *f; bool valid; unsigned verifies; } source_fake;
static tp_result source_verify(void *ctx,const tp_download_manifest *m) {
    source_fake *s=ctx; s->verifies++;
    return s->valid && m->bank==1 && m->version==2 && m->size==512 && m->id[0]==42
        ? TP_OK : TP_INVALID;
}
static tp_result source_read(void *ctx,uint8_t bank,uint32_t offset,void *out,size_t n) {
    source_fake *s=ctx; assert(bank==1 && offset+n<=512);
    memcpy(out,s->f->expected+offset,n); return TP_OK;
}
int main(void) {
    fake f; tp_download_io dest=setup(&f); tp_download_manifest m=manifest();
    source_fake sf={&f,false,0}; tp_staged_source source={&sf,source_verify,source_read};
    tp_staged_copy copy; tp_download_state state;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_INVALID);
    assert(tp_download_read(&dest,&state)==TP_INVALID && f.banks[1][0]==0x55);
    sf.valid=true;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
    assert(tp_staged_copy_step(&copy)==TP_BUSY);
    /* Interrupted write is retried from the persisted offset after restart. */
    f.fail=f.op+5;
    assert(tp_staged_copy_step(&copy)==TP_IO_ERROR);
    assert(tp_staged_copy_step(&copy)==TP_INVALID); f.fail=0;
    assert(tp_download_read(&dest,&state)==TP_OK && state.written==256);
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
    assert(tp_staged_copy_step(&copy)==TP_BUSY);
    assert(tp_staged_copy_step(&copy)==TP_OK);
    assert(tp_download_read(&dest,&state)==TP_OK && state.phase==TP_DOWNLOAD_VERIFIED);
    assert(!memcmp(f.expected,f.banks[1],512));
    for(size_t i=0;i<512;i++) assert(f.banks[0][i]==0x55);
    /* Verified metadata cannot conceal later damage. */
    f.banks[1][0]^=1;
    assert(tp_staged_copy_step(&copy)==TP_INVALID);
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
    assert(tp_staged_copy_step(&copy)==TP_INVALID);
    tp_download_manifest other=m; other.id[1]=1;
    assert(tp_staged_copy_init(&copy,&source,&dest,&other)==TP_BUSY);
    sf.valid=false;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_INVALID);
    assert(sf.verifies>=4);
    /* Inject failures across metadata reads, partial target writes and commits. */
    for(int at=1;at<=9;at++) {
        dest=setup(&f); sf=(source_fake){&f,true,0};
        assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
        f.op=0; f.fail=at;
        assert(tp_staged_copy_step(&copy)==TP_IO_ERROR);
        f.fail=0;
        assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
        tp_result r; do { r=tp_staged_copy_step(&copy); } while(r==TP_BUSY);
        assert(r==TP_OK && !memcmp(f.expected,f.banks[1],512));
    }
    return 0;
}
