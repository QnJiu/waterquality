#include "tp_ds3231.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t reg[256]; int reads; int unstable; int fail; } fake;
static tp_result write_i2c(void *ctx,uint8_t addr,const uint8_t *data,size_t n) {
    fake *f=ctx; assert(addr==0x68 && n>=2); if (f->fail) return TP_IO_ERROR;
    memcpy(f->reg+data[0],data+1,n-1); return TP_OK;
}
static tp_result read_i2c(void *ctx,uint8_t addr,const uint8_t *tx,size_t tn,uint8_t *rx,size_t n) {
    fake *f=ctx; assert(addr==0x68 && tn==1); if (f->fail) return TP_IO_ERROR;
    memcpy(rx,f->reg+tx[0],n);
    if (f->unstable && tx[0]==0) rx[0]=(uint8_t)(++f->reads & 1);
    return TP_OK;
}
int main(void) {
    fake f={0}; tp_i2c_io io={&f,write_i2c,read_i2c}; tp_ds3231 rtc;
    assert(tp_ds3231_init(&rtc,&io)==TP_OK);
    int64_t cases[]={946684800LL,951782400LL,1709251199LL,4102444799LL};
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        int64_t result=0;
        assert(tp_ds3231_set_utc(&rtc,cases[i])==TP_OK);
        assert(tp_ds3231_read_utc(&rtc,&result)==TP_OK && result==cases[i]);
    }
    assert(tp_ds3231_set_utc(&rtc,946684799LL)==TP_INVALID);
    assert(tp_ds3231_set_utc(&rtc,4102444800LL)==TP_INVALID);
    assert(tp_ds3231_set_utc(&rtc,946684800LL)==TP_OK);
    int64_t result=17;
    f.reg[0x0f]|=0x80; assert(tp_ds3231_read_utc(&rtc,&result)==TP_INVALID && result==17);
    f.reg[0x0f]&=0x7f; f.reg[4]=0x30; f.reg[5]=0x02;
    assert(tp_ds3231_read_utc(&rtc,&result)==TP_INVALID && result==17);
    assert(tp_ds3231_set_utc(&rtc,946684800LL)==TP_OK);
    f.reg[2]=0x52; assert(tp_ds3231_read_utc(&rtc,&result)==TP_OK && result==946684800LL);
    f.reg[2]=0x72; assert(tp_ds3231_read_utc(&rtc,&result)==TP_OK && result==946728000LL);
    f.unstable=1; assert(tp_ds3231_read_utc(&rtc,&result)==TP_BUSY);
    f.fail=1; assert(tp_ds3231_read_utc(&rtc,&result)==TP_IO_ERROR);
    return 0;
}
