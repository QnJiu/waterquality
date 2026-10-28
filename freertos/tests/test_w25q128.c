#include "tp_w25q128.h"
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
    puts("W25Q128 checks passed"); return 0;
}
