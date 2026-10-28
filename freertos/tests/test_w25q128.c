#include "tp_w25q128_image.h"
#include "tp_download.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t memory[TP_W25Q128_SIZE];
typedef struct { bool busy,wel,deny_wel,protect,fail,wrong_id; unsigned operations; } fake;
static tp_result spi(void *ctx,const uint8_t *tx,uint8_t *rx,size_t n) {
    fake *f=ctx; if(f->fail) return TP_IO_ERROR;
    memset(rx,0,n);
    if(tx[0]==0x9f && n==4) { rx[1]=0xef; rx[2]=0x40; rx[3]=f->wrong_id?0:0x18; return TP_OK; }
    if(tx[0]==0x05 && n==2) { rx[1]=(f->busy?1:0)|(f->wel?2:0); return TP_OK; }
    if(tx[0]==0x06 && n==1) { f->wel=!f->deny_wel; return TP_OK; }
    assert(n>=4); uint32_t a=((uint32_t)tx[1]<<16)|((uint32_t)tx[2]<<8)|tx[3];
    if(tx[0]==0x03) { memcpy(rx+4,memory+a,n-4); return TP_OK; }
    assert(f->wel && !f->busy); f->wel=false; f->busy=true; f->operations++;
    if(tx[0]==0x02 && !f->protect) for(size_t i=4;i<n;i++) memory[a+i-4]&=tx[i];
    else if(tx[0]==0x20 && !f->protect) memset(memory+a,0xff,TP_W25Q128_SECTOR_SIZE);
    return TP_OK;
}
typedef struct { fake *f; uint32_t now; bool stuck; } clock_state;
static uint32_t now_ms(void *ctx) { return ((clock_state *)ctx)->now; }
static void sleep_ms(void *ctx) {
    clock_state *c=ctx; c->now++; if(!c->stuck) c->f->busy=false;
}
static uint8_t metadata[2][128];
static tp_result meta_read(void *ctx,unsigned slot,uint32_t offset,void *out,size_t n) {
    (void)ctx; if(slot>1 || offset>128 || n>128-offset) return TP_INVALID;
    memcpy(out,metadata[slot]+offset,n); return TP_OK;
}
static tp_result meta_erase(void *ctx,unsigned slot) {
    (void)ctx; if(slot>1) return TP_INVALID; memset(metadata[slot],255,128); return TP_OK;
}
static tp_result meta_write(void *ctx,unsigned slot,uint32_t offset,const void *src,size_t n) {
    (void)ctx; if(slot>1 || offset>128 || n>128-offset) return TP_INVALID;
    memcpy(metadata[slot]+offset,src,n); return TP_OK;
}
static tp_result verify_image(void *ctx,const tp_download_manifest *manifest) {
    uint8_t got[3]; const uint8_t expected[3]={0x12,0x34,0x56};
    return manifest->size==3 && tp_w25q128_image_read(ctx,manifest->bank,0,got,3)==TP_OK &&
        !memcmp(got,expected,3) ? TP_OK : TP_IO_ERROR;
}
int main(void) {
    memset(memory,0xff,sizeof(memory)); fake f={0}; tp_w25q128 flash;
    tp_w25q128_io io={&f,spi}; uint8_t bytes[]={0x12,0x34,0x56}, out[3];
    f.wrong_id=true; assert(tp_w25q128_init(&flash,&io)==TP_UNSUPPORTED); f.wrong_id=false;
    f.busy=true; assert(tp_w25q128_init(&flash,&io)==TP_BUSY); f.busy=false;
    assert(tp_w25q128_init(&flash,&io)==TP_OK);
    assert(tp_w25q128_program(&flash,255,bytes,3,0,20)==TP_INVALID);
    assert(tp_w25q128_erase(&flash,1,0,20)==TP_INVALID);
    assert(tp_w25q128_read(&flash,TP_W25Q128_SIZE-2,out,3)==TP_INVALID);
    assert(tp_w25q128_program(&flash,256,bytes,3,0xfffffffcu,20)==TP_OK);
    assert(tp_w25q128_read(&flash,256,out,3)==TP_BUSY);
    assert(tp_w25q128_poll(&flash,1)==TP_BUSY); f.busy=false;
    assert(tp_w25q128_poll(&flash,2)==TP_OK);
    assert(tp_w25q128_read(&flash,256,out,3)==TP_OK && !memcmp(bytes,out,3));
    assert(tp_w25q128_erase(&flash,0,10,20)==TP_OK); f.busy=false;
    assert(tp_w25q128_poll(&flash,11)==TP_OK);
    assert(tp_w25q128_read(&flash,256,out,3)==TP_OK && out[0]==0xff);
    f.deny_wel=true; assert(tp_w25q128_program(&flash,0,bytes,3,20,10)==TP_IO_ERROR);
    assert(tp_w25q128_read(&flash,0,out,3)==TP_INVALID); f.deny_wel=false;
    assert(tp_w25q128_init(&flash,&io)==TP_OK);
    f.protect=true; assert(tp_w25q128_program(&flash,0,bytes,3,30,10)==TP_OK);
    f.busy=false; assert(tp_w25q128_poll(&flash,31)==TP_OK);
    assert(tp_w25q128_read(&flash,0,out,3)==TP_OK && memcmp(bytes,out,3));
    f.protect=false; assert(tp_w25q128_program(&flash,0,bytes,3,40,10)==TP_OK);
    assert(tp_w25q128_poll(&flash,50)==TP_TIMEOUT && !flash.ready);
    assert(tp_w25q128_init(&flash,&io)==TP_BUSY); f.busy=false;
    assert(tp_w25q128_init(&flash,&io)==TP_OK); f.fail=true;
    assert(tp_w25q128_read(&flash,0,out,3)==TP_IO_ERROR && !flash.ready);
    f.fail=false; assert(tp_w25q128_init(&flash,&io)==TP_OK);
    clock_state clock={.f=&f}; tp_w25q128_image image;
    assert(tp_w25q128_image_init(&image,&flash,&clock,now_ms,sleep_ms,0,0,4096,0,10,10)==TP_INVALID);
    assert(tp_w25q128_image_init(&image,&flash,&clock,now_ms,sleep_ms,0,4096,4096,0,10,10)==TP_OK);
    assert(tp_w25q128_image_write(&image,0,0,bytes,3)==TP_INVALID);
    assert(tp_w25q128_image_erase(&image,1)==TP_OK);
    unsigned before=f.operations;
    assert(tp_w25q128_image_write(&image,1,255,bytes,3)==TP_OK && f.operations==before+2);
    assert(tp_w25q128_image_read(&image,1,255,out,3)==TP_OK && !memcmp(bytes,out,3));
    before=f.operations;
    assert(tp_w25q128_image_write(&image,1,255,bytes,3)==TP_OK && f.operations==before);
    uint8_t ones[3]={255,255,255};
    assert(tp_w25q128_image_write(&image,1,255,ones,3)==TP_INVALID && f.operations==before);
    f.protect=true;
    assert(tp_w25q128_image_erase(&image,1)==TP_IO_ERROR); f.protect=false;
    assert(tp_w25q128_image_erase(&image,1)==TP_OK);
    clock.stuck=true;
    assert(tp_w25q128_image_write(&image,1,0,bytes,3)==TP_TIMEOUT && !flash.ready);
    f.busy=false; clock.stuck=false; assert(tp_w25q128_init(&flash,&io)==TP_OK);
    assert(tp_w25q128_image_write(&image,1,0,bytes,3)==TP_OK);
    assert(tp_w25q128_image_set_protected(&image,1)==TP_OK);
    assert(tp_w25q128_image_erase(&image,1)==TP_INVALID);
    assert(tp_w25q128_image_set_protected(&image,0)==TP_OK);
    memset(metadata,255,sizeof(metadata));
    tp_download_io download={{NULL,meta_read,meta_erase,meta_write},&image,
        tp_w25q128_image_erase,tp_w25q128_image_read,tp_w25q128_image_write,verify_image};
    tp_download_manifest manifest={.version=1,.size=3,.bank=1};
    assert(tp_download_begin(&download,&manifest)==TP_OK);
    assert(tp_download_write(&download,0,bytes,2)==TP_OK);
    assert(tp_download_resume(&download)==TP_OK);
    assert(tp_download_write(&download,2,bytes+2,1)==TP_OK);
    assert(tp_download_finish(&download)==TP_OK);
    tp_download_state state;
    assert(tp_download_read(&download,&state)==TP_OK && state.phase==TP_DOWNLOAD_VERIFIED);
    puts("W25Q128 checks passed"); return 0;
}
