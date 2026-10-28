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
#include "tp_staged_upgrade.h"

int main(void) {
    fixture f; init(&f.target,&f.boot); memcpy(f.source,"abc",3); f.erases=0;
    fake boot_store; tp_signed_boot unused; init(&boot_store,&unused);
    tp_secure_upgrade_config config={0};
    config.boot_metadata=(tp_store_io){&boot_store,read_meta,erase_meta,write_meta};
    config.download_metadata=(tp_store_io){&f.target,read_meta,erase_meta,write_meta};
    config.manifests=(tp_store_io){&f.target,read_manifest,erase_manifest,write_manifest};
    config.image_ctx=&f; config.erase_image=dest_erase; config.read_image=dest_read;
    config.write_image=dest_write; config.capacity[0]=config.capacity[1]=3;
    config.product_id=f.boot.signature.product_id;
    memcpy(config.public_key,f.boot.signature.public_key,64);
    tp_secure_upgrade u; assert(tp_secure_upgrade_init(&u,&config)==TP_OK);
    const char *v0="5450553178563412010000000300000000000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad951584689bde05b6bcb46ada8a291c99c8636798711a92c0b11b3da52d664e42a40bb63cca6e220774961b8f1eb8b1a1b71ec31c225c163e8a3c24e8003fb2d7";
    uint8_t signed0[116],signed1[116]; hex(v0,signed0,116); hex(signed_v1,signed1,116);
    tp_download_manifest m;
    assert(tp_signed_boot_authenticate(&u.signed_boot,signed0,116,&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&u.signed_boot,&m)==TP_OK);
    assert(tp_boot_provision(&u.boot,0,1)==TP_OK);
    memset(f.target.image[1],0x55,3);
    tp_image_hash_io image={&f,source_read,{3,3}}; tp_staged_upgrade s;
    assert(tp_staged_upgrade_init(&s,&u,&image,signed0,116)==TP_INVALID && f.erases==0);
    f.source[0]^=1;
    assert(tp_staged_upgrade_init(&s,&u,&image,signed1,116)==TP_INVALID && f.erases==0);
    f.source[0]^=1;
    assert(tp_staged_upgrade_init(&s,&u,&image,signed1,116)==TP_OK && f.erases==1);
    assert(tp_staged_upgrade_finish(&s)==TP_INVALID);
    assert(tp_staged_upgrade_step(&s)==TP_BUSY);
    /* Recreate both objects after a reset; persistent records resume the copy. */
    assert(tp_secure_upgrade_init(&u,&config)==TP_OK);
    assert(tp_staged_upgrade_init(&s,&u,&image,signed1,116)==TP_OK && f.erases==1);
    assert(tp_staged_upgrade_step(&s)==TP_OK);
    assert(tp_signed_boot_verify(&u.signed_boot,1,2)==TP_OK);
    f.target.image[1][0]^=1;
    assert(tp_staged_upgrade_finish(&s)==TP_INVALID);
    f.target.image[1][0]^=1;
    assert(tp_staged_upgrade_init(&s,&u,&image,signed1,116)==TP_OK);
    assert(tp_staged_upgrade_step(&s)==TP_OK);
    assert(tp_staged_upgrade_finish(&s)==TP_OK);
    tp_boot_state state; assert(tp_boot_read(&u.boot,&state)==TP_OK && state.phase==TP_BOOT_READY);
    assert(tp_staged_upgrade_init(&s,&u,&image,signed1,116)==TP_BUSY && f.erases==1);
    uint8_t selected=0; assert(tp_boot_select(&u.boot,&selected)==TP_OK && selected==1);
    assert(tp_boot_select(&u.boot,&selected)==TP_OK && selected==0);
    return 0;
}
