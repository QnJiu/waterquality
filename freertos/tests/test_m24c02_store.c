#include "tp_m24c02.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t mem[256]; int writes, fail_at; bool protect; } fake;
static tp_result wr(void *ctx,uint8_t addr,const uint8_t *tx,size_t n) {
    fake *f=ctx; assert(addr==0x52 && n>=2 && n<=9);
    assert(tx[0]/8==(tx[0]+n-2)/8);
    int current=f->writes++;
    if (current==f->fail_at) {
        /* Model power loss after an arbitrary prefix of this transfer. */
        if (!f->protect) memcpy(f->mem+tx[0],tx+1,(n-1)/2);
        return TP_IO_ERROR;
    }
    if (!f->protect) memcpy(f->mem+tx[0],tx+1,n-1);
    return TP_OK;
}
static tp_result rd(void *ctx,uint8_t addr,const uint8_t *tx,size_t tn,uint8_t *rx,size_t n) {
    fake *f=ctx; assert(addr==0x52 && tn==1 && (size_t)tx[0]+n<=256);
    memcpy(rx,f->mem+tx[0],n); return TP_OK;
}
static tp_result wait_done(void *ctx,uint8_t addr) { (void)ctx; assert(addr==0x52); return TP_OK; }
int main(void) {
    fake f; memset(&f,0xff,sizeof(f)); f.writes=0; f.fail_at=-1; f.protect=false;
    tp_i2c_io bus={&f,wr,rd}; tp_m24c02 dev; assert(tp_m24c02_init(&dev,&bus,0x52,&f,wait_done)==TP_OK);
    tp_store_io io=tp_m24c02_store(&dev);
    tp_config a={.calibration={1,0,0,0.01f,10},.kp=1,.output_max=1,.integral_max=10,
                 .max_temperature_c=100,.period_seconds=3600,.slave_address=1};
    tp_config b=a, got; b.period_seconds=7200; uint32_t seq;
    assert(tp_store_save(&io,&a)==TP_OK);
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==1 && got.period_seconds==3600);
    uint8_t baseline[256]; memcpy(baseline,f.mem,sizeof(baseline));
    f.writes=0; assert(tp_store_save(&io,&b)==TP_OK); int transactions=f.writes;
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==2 && got.period_seconds==7200);
    for(int i=0;i<transactions;i++) {
        memcpy(f.mem,baseline,sizeof(baseline)); f.writes=0; f.fail_at=i;
        assert(tp_store_save(&io,&b)==TP_IO_ERROR); f.fail_at=-1;
        assert(tp_store_load(&io,&got,&seq)==TP_OK);
        assert((seq==1 && got.period_seconds==3600) || (seq==2 && got.period_seconds==7200));
    }
    /* Reuse a slot with a valid commit and ensure write protection is detected. */
    memcpy(f.mem,baseline,sizeof(baseline)); f.writes=0;
    assert(tp_store_save(&io,&b)==TP_OK); f.protect=true; f.writes=0;
    assert(tp_store_save(&io,&a)==TP_IO_ERROR);
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==2 && got.period_seconds==7200);
    uint8_t byte=0;
    assert(io.read(io.ctx,2,0,&byte,1)==TP_INVALID);
    assert(io.write(io.ctx,0,127,&byte,2)==TP_INVALID);
    puts("M24C02 configuration recovery checks passed"); return 0;
}
