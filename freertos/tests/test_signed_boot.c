#include "tp_signed_boot.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t manifests[2][128], metadata[2][128], image[2][3];
    int op, fail;
    bool image_fail;
} fake;
static tp_result read_manifest(void *v,unsigned slot,uint32_t off,void *out,size_t n) {
    fake *f=v; if(++f->op==f->fail) return TP_IO_ERROR;
    assert(slot<2 && off+n<=128); memcpy(out,f->manifests[slot]+off,n); return TP_OK;
}
static tp_result erase_manifest(void *v,unsigned slot) {
    fake *f=v; if(++f->op==f->fail) return TP_IO_ERROR;
    assert(slot<2); memset(f->manifests[slot],0xff,128); return TP_OK;
}
static tp_result write_manifest(void *v,unsigned slot,uint32_t off,const void *in,size_t n) {
    fake *f=v; if(++f->op==f->fail) return TP_IO_ERROR;
    assert(slot<2 && off+n<=128); memcpy(f->manifests[slot]+off,in,n); return TP_OK;
}
static tp_result read_meta(void *v,unsigned slot,uint32_t off,void *out,size_t n) {
    fake *f=v; assert(slot<2 && off+n<=128); memcpy(out,f->metadata[slot]+off,n); return TP_OK;
}
static tp_result erase_meta(void *v,unsigned slot) {
    fake *f=v; assert(slot<2); memset(f->metadata[slot],0xff,128); return TP_OK;
}
static tp_result write_meta(void *v,unsigned slot,uint32_t off,const void *in,size_t n) {
    fake *f=v; assert(slot<2 && off+n<=128); memcpy(f->metadata[slot]+off,in,n); return TP_OK;
}
static tp_result read_image(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fake *f=v; if(f->image_fail) return TP_IO_ERROR;
    assert(bank<2 && off+n<=3); memcpy(out,f->image[bank]+off,n); return TP_OK;
}
static void hex(const char *s,uint8_t *out,size_t n) {
    for(size_t i=0;i<n;i++) { unsigned x=0; for(unsigned j=0;j<2;j++) {
        char c=*s++; x=x*16+(unsigned)(c<='9'?c-'0':c-'a'+10); } out[i]=(uint8_t)x; }
}
static void init(fake *f,tp_signed_boot *ctx) {
    memset(f,0,sizeof(*f)); memset(f->manifests,0xff,sizeof(f->manifests));
    memset(f->metadata,0xff,sizeof(f->metadata)); memcpy(f->image[0],"abc",3); memcpy(f->image[1],"abc",3);
    memset(ctx,0,sizeof(*ctx));
    hex("6780c5fc70275e2c7061a0e7877bb174deadeb9887027f3fa83654158ba7f50c3cba8c34bc35d20e81f730ac1c7bd6d661a942f90c6a9ca55c512f9e4a001266",ctx->signature.public_key,64);
    ctx->signature.product_id=0x12345678u;
    ctx->signature.image=(tp_image_hash_io){f,read_image,{3,3}};
    ctx->manifests=(tp_store_io){f,read_manifest,erase_manifest,write_manifest};
}
int main(void) {
    /* Public test key (private scalar 42); never deploy. */
    const char *v0="5450553178563412010000000300000000000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad951584689bde05b6bcb46ada8a291c99c8636798711a92c0b11b3da52d664e42a40bb63cca6e220774961b8f1eb8b1a1b71ec31c225c163e8a3c24e8003fb2d7";
    const char *v1="5450553178563412020000000300000001000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015adad628c012e2dd70b93c667ef7166ff7dbf199f87e4105fbf15f89ba483f0e7f169b5287eedeef89d4a6299c0fc066a9a94b7418f92722a08f81f22d5b9f12679";
    uint8_t data0[116],data1[116]; hex(v0,data0,116); hex(v1,data1,116);
    fake f; tp_signed_boot ctx; tp_download_manifest m; init(&f,&ctx);
    assert(tp_signed_boot_authenticate(&ctx,data0,116,&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&ctx,&m)==TP_OK);
    tp_boot_io boot={{&f,read_meta,erase_meta,write_meta},&ctx,tp_signed_boot_verify};
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    assert(tp_signed_boot_authenticate(&ctx,data1,116,&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&ctx,&m)==TP_OK);
    memset(&ctx.signature.authenticated,0,sizeof(ctx.signature.authenticated)); ctx.signature.ready=false; ctx.has_manifest=false;
    assert(tp_signed_boot_verify(&ctx,1,2)==TP_OK);
    assert(tp_signed_boot_verify(&ctx,1,3)==TP_INVALID);
    assert(tp_boot_stage(&boot,1,2)==TP_OK);
    uint8_t selected=99; assert(tp_boot_select(&boot,&selected)==TP_OK && selected==1);
    assert(tp_boot_select(&boot,&selected)==TP_OK && selected==0);
    f.manifests[1][52]^=1; assert(tp_signed_boot_verify(&ctx,1,2)==TP_INVALID); f.manifests[1][52]^=1;
    f.image[1][0]^=1; assert(tp_signed_boot_verify(&ctx,1,2)==TP_INVALID); f.image[1][0]^=1;
    f.image_fail=true; assert(tp_signed_boot_verify(&ctx,1,2)==TP_IO_ERROR); f.image_fail=false;
    memcpy(f.manifests[0],f.manifests[1],120); assert(tp_signed_boot_verify(&ctx,0,2)==TP_INVALID);
    for(int at=1;at<=6;at++) {
        init(&f,&ctx); assert(tp_signed_boot_authenticate(&ctx,data1,116,&m)==TP_OK);
        f.op=0; f.fail=at; assert(tp_signed_boot_download_verify(&ctx,&m)==TP_IO_ERROR);
        f.fail=0;
        tp_result r=tp_signed_boot_verify(&ctx,1,2);
        assert(r==TP_INVALID || r==TP_OK); /* final read may fail after commit */
        assert(tp_signed_boot_download_verify(&ctx,&m)==TP_OK);
        assert(tp_signed_boot_verify(&ctx,1,2)==TP_OK);
    }
    return 0;
}
