#include "tp_m24c02.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t data[256]; unsigned writes, waits, reads; int fail_write, fail_wait, fail_read; bool busy; } fake;
static tp_result write_bus(void *ctx, uint8_t address, const uint8_t *tx, size_t n) {
    fake *f=ctx; assert(address==0x53 && n>=2 && n<=9 && !f->busy);
    assert((tx[0] / 8u) == ((tx[0] + n - 2u) / 8u));
    if ((int)f->writes++ == f->fail_write) return TP_IO_ERROR;
    memcpy(f->data+tx[0],tx+1,n-1); f->busy=true; return TP_OK;
}
static tp_result read_bus(void *ctx, uint8_t address, const uint8_t *tx, size_t tn, uint8_t *rx, size_t n) {
    fake *f=ctx; assert(address==0x53 && tn==1 && !f->busy); f->reads++;
    if (f->fail_read) return TP_IO_ERROR;
    memcpy(rx,f->data+tx[0],n); return TP_OK;
}
static tp_result wait_ready(void *ctx, uint8_t address) {
    fake *f=ctx; assert(address==0x53 && f->busy);
    if ((int)f->waits++ == f->fail_wait) return TP_IO_ERROR;
    f->busy=false; return TP_OK;
}
int main(void) {
    fake f={.fail_write=-1,.fail_wait=-1}; tp_i2c_io bus={&f,write_bus,read_bus}; tp_m24c02 d;
    assert(tp_m24c02_init(&d,&bus,0x4f,&f,wait_ready)==TP_INVALID);
    assert(tp_m24c02_init(&d,&bus,0x53,&f,NULL)==TP_INVALID);
    assert(tp_m24c02_init(&d,&bus,0x53,&f,wait_ready)==TP_OK);
    uint8_t input[20], output[20]; for (unsigned i=0;i<20;i++) input[i]=(uint8_t)(i+1);
    assert(tp_m24c02_write(&d,6,input,sizeof(input))==TP_OK);
    assert(f.writes==4 && f.waits==4);
    assert(tp_m24c02_read(&d,6,output,sizeof(output))==TP_OK && !memcmp(input,output,sizeof(input)));
    assert(tp_m24c02_write(&d,255,input,1)==TP_OK);
    assert(tp_m24c02_read(&d,255,output,1)==TP_OK && output[0]==input[0]);
    assert(tp_m24c02_write(&d,255,input,2)==TP_INVALID);
    assert(tp_m24c02_read(&d,256,output,1)==TP_INVALID);
    assert(tp_m24c02_read(&d,256,NULL,0)==TP_OK);
    assert(tp_m24c02_write(&d,0,NULL,1)==TP_INVALID);
    f.fail_write=(int)f.writes; unsigned waits=f.waits;
    assert(tp_m24c02_write(&d,0,input,1)==TP_IO_ERROR && f.waits==waits);
    f.fail_write=-1; f.fail_wait=(int)f.waits;
    assert(tp_m24c02_write(&d,0,input,20)==TP_IO_ERROR && f.busy);
    f.busy=false; f.fail_read=1;
    assert(tp_m24c02_read(&d,0,output,1)==TP_IO_ERROR);
    puts("M24C02 page and error checks passed"); return 0;
}
