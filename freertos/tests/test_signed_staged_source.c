#define main signed_boot_original_main
#include "test_signed_boot.c"
#undef main
#include "tp_signed_staged_source.h"

static const char *signed_v1="5450553178563412020000000300000001000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015adad628c012e2dd70b93c667ef7166ff7dbf199f87e4105fbf15f89ba483f0e7f169b5287eedeef89d4a6299c0fc066a9a94b7418f92722a08f81f22d5b9f12679";
typedef struct { fake target; tp_signed_boot boot; uint8_t source[3]; unsigned erases; } fixture;
static tp_result source_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(out,f->source+off,n); return TP_OK;
}
static tp_result dest_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; return read_image(&f->target,bank,off,out,n);
}
static tp_result dest_erase(void *v,uint8_t bank) {
    fixture *f=v; if(bank!=1) return TP_INVALID;
    f->erases++; memset(f->target.image[bank],0xff,3); return TP_OK;
}
static tp_result dest_write(void *v,uint8_t bank,uint32_t off,const void *in,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(f->target.image[bank]+off,in,n); return TP_OK;
}
static tp_result dest_verify(void *v,const tp_download_manifest *m) {
    fixture *f=v; return tp_signed_boot_download_verify(&f->boot,m);
}
int main(void) {
    fixture f; init(&f.target,&f.boot); memcpy(f.source,"abc",3); f.erases=0;
    memset(f.target.image[1],0x55,3);
    uint8_t bytes[116]; hex(signed_v1,bytes,sizeof(bytes));
    tp_signed_staged_source s={0}; s.signature=f.boot.signature;
    s.signature.image=(tp_image_hash_io){&f,source_read,{3,3}};
    s.manifest_bytes=bytes; s.manifest_size=sizeof(bytes);
    tp_staged_source source=tp_signed_staged_source_callbacks(&s);
    tp_download_io dest={{&f.target,read_meta,erase_meta,write_meta},&f,
                         dest_erase,dest_read,dest_write,dest_verify};
    tp_download_manifest m;
    assert(tp_signed_boot_authenticate(&f.boot,bytes,sizeof(bytes),&m)==TP_OK);
    tp_staged_copy copy;
    f.source[0]^=1;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_INVALID);
    assert(f.erases==0 && f.target.image[1][0]==0x55);
    f.source[0]^=1;
    bytes[52]^=1;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_INVALID && f.erases==0);
    bytes[52]^=1;
    tp_download_manifest wrong=m; wrong.version++;
    assert(tp_staged_copy_init(&copy,&source,&dest,&wrong)==TP_INVALID && f.erases==0);
    s.signature.product_id++;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_INVALID && f.erases==0);
    s.signature.product_id--;
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK && f.erases==1);
    assert(tp_staged_copy_step(&copy)==TP_BUSY);
    /* Simulate reset and restore both independent authentication contexts. */
    s.signature.ready=false; f.boot.has_manifest=false; f.boot.signature.ready=false;
    assert(tp_signed_boot_authenticate(&f.boot,bytes,sizeof(bytes),&m)==TP_OK);
    assert(tp_staged_copy_init(&copy,&source,&dest,&m)==TP_OK);
    assert(tp_staged_copy_step(&copy)==TP_OK);
    assert(!memcmp(f.target.image[1],"abc",3));
    assert(tp_signed_boot_verify(&f.boot,1,2)==TP_OK);
    f.target.image[1][0]^=1;
    assert(tp_signed_boot_verify(&f.boot,1,2)==TP_INVALID);
    assert(tp_staged_copy_step(&copy)==TP_INVALID);
    return 0;
}
