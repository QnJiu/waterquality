#include "tp_pn532_i2c.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t tx[TP_PN532_FRAME_MAX], frames[2][32];
    size_t tx_len, sizes[2], index, offset, count;
    bool held, final, busy_once, ready, fail, aborted;
    unsigned stops;
} fake;
static tp_result write_start(void *ctx,uint8_t address,const uint8_t *data,size_t n) {
    fake *f=ctx; assert(address==TP_PN532_I2C_ADDRESS); memcpy(f->tx,data,n); f->tx_len=n;
    return f->fail ? TP_IO_ERROR : TP_OK;
}
static tp_result write_poll(void *ctx) {
    fake *f=ctx; if(f->busy_once) { f->busy_once=false; return TP_BUSY; }
    return f->fail ? TP_IO_ERROR : TP_OK;
}
static tp_result read_start(void *ctx,uint8_t address,size_t n,bool final) {
    fake *f=ctx; assert(address==TP_PN532_I2C_ADDRESS && n==1 && !final && !f->held);
    f->held=true; f->offset=0; f->count=n; f->final=final; return TP_OK;
}
static tp_result read_continue(void *ctx,size_t n,bool final) {
    fake *f=ctx; assert(f->held); f->count=n; f->final=final; return TP_OK;
}
static tp_result read_poll(void *ctx,uint8_t *out,size_t n) {
    fake *f=ctx; assert(f->held && n==f->count);
    if(f->fail) return TP_IO_ERROR;
    if(f->offset==0) { assert(n==1); out[0]=f->ready ? 1 : 0; f->offset=1; return TP_OK; }
    assert(f->index<2 && f->offset-1+n<=f->sizes[f->index]);
    memcpy(out,f->frames[f->index]+f->offset-1,n); f->offset+=n;
    if(f->final) { assert(f->offset-1==f->sizes[f->index]); f->held=false; f->index++; }
    return TP_OK;
}
static void read_stop(void *ctx) { fake *f=ctx; assert(f->held); f->held=false; f->stops++; }
static void abort_bus(void *ctx) { fake *f=ctx; f->held=false; f->aborted=true; }
static tp_pn532_i2c_bus bus(fake *f) {
    tp_pn532_i2c_bus b={f,write_start,write_poll,read_start,read_continue,read_poll,read_stop,abort_bus}; return b;
}
int main(void) {
    fake f={0}; tp_pn532_i2c transport; tp_pn532_i2c_bus b=bus(&f);
    const uint8_t ack[]={0,0,0xff,0,0xff,0};
    const uint8_t response[]={0,0,0xff,3,0xfd,0xd5,0x03,0x42,0xe6,0};
    memcpy(f.frames[0],ack,sizeof(ack)); f.sizes[0]=sizeof(ack);
    memcpy(f.frames[1],response,sizeof(response)); f.sizes[1]=sizeof(response);
    assert(tp_pn532_i2c_init(&transport,&b)==TP_OK);
    tp_pn532_io io=tp_pn532_i2c_io(&transport); tp_pn532 device;
    assert(tp_pn532_init(&device,&io)==TP_OK);
    f.busy_once=true;
    assert(tp_pn532_start(&device,0,100,2,NULL,0)==TP_OK && f.tx_len==9);
    assert(tp_pn532_tick(&device,1)==TP_BUSY);
    assert(tp_pn532_tick(&device,2)==TP_BUSY);
    assert(tp_pn532_tick(&device,3)==TP_BUSY && f.stops==1 && !f.held);
    f.ready=true;
    for(uint32_t now=4;now<20 && device.state!=TP_PN532_DONE;now++) {
        tp_result r=tp_pn532_tick(&device,now); assert(r==TP_BUSY || r==TP_OK);
    }
    uint8_t data[4]; size_t len=0;
    assert(device.state==TP_PN532_DONE && f.index==2 && !f.held);
    assert(tp_pn532_take(&device,data,sizeof(data),&len)==TP_OK && len==1 && data[0]==0x42);

    f.index=0; f.frames[0][3]=1; f.frames[0][4]=0xfe; f.ready=true; f.aborted=false;
    assert(tp_pn532_start(&device,20,100,2,NULL,0)==TP_OK);
    assert(tp_pn532_tick(&device,21)==TP_BUSY);
    assert(tp_pn532_tick(&device,22)==TP_BUSY);
    assert(tp_pn532_tick(&device,23)==TP_INVALID && f.aborted && device.state==TP_PN532_DESYNC);
    assert(transport.state==TP_PN532_I2C_FAULT);

    f.fail=false; f.aborted=false; assert(tp_pn532_i2c_init(&transport,&b)==TP_OK);
    f.fail=true; assert(io.send(io.ctx,ack,sizeof(ack))==TP_IO_ERROR && f.aborted);
    f.fail=false; f.aborted=false; assert(tp_pn532_i2c_init(&transport,&b)==TP_OK);
    tp_pn532_i2c_abort(&transport); assert(f.aborted && transport.state==TP_PN532_I2C_FAULT);
    return 0;
}
