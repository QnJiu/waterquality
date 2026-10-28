#include "tp_image_hash.h"
#include <assert.h>
#include <string.h>
static uint8_t banks[2][1000];
static unsigned reads;
static bool fail;
static tp_result read_bank(void *ctx,uint8_t bank,uint32_t offset,void *out,size_t n) {
    (void)ctx; reads++;
    if (fail && offset>=256) return TP_IO_ERROR;
    assert(bank<2 && offset<=1000 && n<=1000-offset);
    memcpy(out,banks[bank]+offset,n); return TP_OK;
}
int main(void) {
    static const uint8_t abc_hash[32]={
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static const uint8_t thousand_a[32]={
        0x41,0xed,0xec,0xe4,0x2d,0x63,0xe8,0xd9,0xbf,0x51,0x5a,0x9b,0xa6,0x93,0x2e,0x1c,
        0x20,0xcb,0xc9,0xf5,0xa5,0xd1,0x34,0x64,0x5a,0xdb,0x5d,0xb1,0xb9,0x73,0x7e,0xa3};
    tp_image_hash_io io={NULL,read_bank,{1000,1000}}; uint8_t out[32];
    memcpy(banks[0],"abc",3); memset(banks[1],'a',1000);
    assert(tp_image_sha256(&io,0,3,out)==TP_OK && memcmp(out,abc_hash,32)==0);
    reads=0; assert(tp_image_sha256_verify(&io,1,1000,thousand_a)==TP_OK && reads==4);
    banks[1][511]^=1; assert(tp_image_sha256_verify(&io,1,1000,thousand_a)==TP_INVALID);
    fail=true; assert(tp_image_sha256_verify(&io,1,1000,thousand_a)==TP_IO_ERROR); fail=false;
    reads=0; assert(tp_image_sha256(&io,2,3,out)==TP_INVALID && reads==0);
    assert(tp_image_sha256(&io,1,1001,out)==TP_INVALID && reads==0);
    assert(tp_image_sha256(&io,1,0,out)==TP_INVALID && reads==0);
    assert(tp_image_sha256_verify(&io,0,3,NULL)==TP_INVALID);
    return 0;
}
