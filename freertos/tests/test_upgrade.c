#include "tp_upgrade.h"
#include <assert.h>
#include <string.h>

typedef struct { uint8_t slot[2][128]; bool fail_commit_read, committed; } store;
typedef struct { uint8_t bank[2][16]; unsigned erases, writes; bool reject; } images;
static tp_result read_slot(void *ctx,unsigned slot,uint32_t off,void *out,size_t n) {
    store *s=ctx; assert(slot<2 && off+n<=128);
    if(s->fail_commit_read && s->committed && off==0 && n==36) {
        s->fail_commit_read=false; return TP_IO_ERROR;
    }
    memcpy(out,s->slot[slot]+off,n); return TP_OK;
}
static tp_result erase_slot(void *ctx,unsigned slot) {
    store *s=ctx; assert(slot<2); memset(s->slot[slot],0xff,128); return TP_OK;
}
static tp_result write_slot(void *ctx,unsigned slot,uint32_t off,const void *in,size_t n) {
    store *s=ctx; const uint8_t *p=in; assert(slot<2 && off+n<=128);
    for(size_t i=0;i<n;i++) { assert((s->slot[slot][off+i]&p[i])==p[i]); s->slot[slot][off+i]&=p[i]; }
    if(off==32 && n==4) s->committed=true;
    return TP_OK;
}
static tp_result erase_image(void *ctx,uint8_t bank) {
    images *i=ctx; assert(bank<2); i->erases++; memset(i->bank[bank],0xff,16); return TP_OK;
}
static tp_result read_image(void *ctx,uint8_t bank,uint32_t off,void *out,size_t n) {
    images *i=ctx; assert(bank<2 && off+n<=16); memcpy(out,i->bank[bank]+off,n); return TP_OK;
}
static tp_result write_image(void *ctx,uint8_t bank,uint32_t off,const void *in,size_t n) {
    images *i=ctx; assert(bank<2 && off+n<=16); i->writes++; memcpy(i->bank[bank]+off,in,n); return TP_OK;
}
static tp_result verify_download(void *ctx,const tp_download_manifest *m) {
    images *i=ctx;
    return !i->reject && m->bank==1 && m->version==2 && m->size==4 &&
        !memcmp(i->bank[1],"data",4) ? TP_OK : TP_INVALID;
}
static tp_result verify_boot(void *ctx,uint8_t bank,uint32_t version) {
    images *i=ctx;
    if(bank==0 && version==1) return TP_OK;
    return !i->reject && bank==1 && version==2 && !memcmp(i->bank[1],"data",4) ? TP_OK : TP_INVALID;
}
int main(void) {
    store bs,ds; images image={0};
    memset(&bs,0,sizeof(bs)); memset(&ds,0,sizeof(ds));
    memset(bs.slot,0xff,sizeof(bs.slot)); memset(ds.slot,0xff,sizeof(ds.slot));
    tp_boot_io boot={{&bs,read_slot,erase_slot,write_slot},&image,verify_boot};
    tp_download_io download={{&ds,read_slot,erase_slot,write_slot},&image,erase_image,read_image,write_image,verify_download};
    tp_upgrade upgrade={&boot,&download};
    tp_download_manifest manifest={{1},2,4,1};
    assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID);
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    manifest.bank=0; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID && image.erases==0);
    manifest.bank=1; manifest.version=1; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID && image.erases==0);
    manifest.version=2; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_OK && image.erases==1);
    assert(tp_upgrade_finish(&upgrade)==TP_INVALID);
    assert(tp_upgrade_write(&upgrade,0,"data",4)==TP_OK);
    image.reject=true; assert(tp_upgrade_finish(&upgrade)==TP_INVALID);
    tp_boot_state state; assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_IDLE);
    image.reject=false; bs.committed=false; bs.fail_commit_read=true;
    assert(tp_upgrade_finish(&upgrade)==TP_IO_ERROR);
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_READY);
    assert(tp_upgrade_finish(&upgrade)==TP_OK);
    unsigned erases=image.erases, writes=image.writes;
    assert(tp_upgrade_begin(&upgrade,&manifest)==TP_BUSY);
    assert(tp_upgrade_resume(&upgrade)==TP_BUSY);
    assert(tp_upgrade_write(&upgrade,0,"data",4)==TP_BUSY);
    assert(image.erases==erases && image.writes==writes);
    uint8_t bank; assert(tp_boot_select(&boot,&bank)==TP_OK && bank==1);
    assert(tp_upgrade_finish(&upgrade)==TP_BUSY);
    assert(tp_upgrade_resume(&upgrade)==TP_BUSY);
    assert(tp_boot_confirm(&boot,1)==TP_OK);
    assert(tp_upgrade_resume(&upgrade)==TP_INVALID);
    assert(image.erases==erases && image.writes==writes);
    return 0;
}
