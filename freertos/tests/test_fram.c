#include "tp_mb85rc16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t mem[2048]; int writes, fail_after; bool read_error; } fake;
static tp_result write_i2c(void *ctx,uint8_t dev,const uint8_t *tx,size_t n) {
    fake *f=ctx; if (f->fail_after>=0 && f->writes++ >= f->fail_after) return TP_IO_ERROR;
    if (dev<0x50 || dev>0x57 || n<2 || (size_t)tx[0]+n-1>256) return TP_IO_ERROR;
    size_t addr=(size_t)(dev-0x50)*256+tx[0]; memcpy(f->mem+addr,tx+1,n-1); return TP_OK;
}
static tp_result read_i2c(void *ctx,uint8_t dev,const uint8_t *tx,size_t tn,uint8_t *rx,size_t n) {
    fake *f=ctx; if (f->read_error || dev<0x50 || dev>0x57 || tn!=1 || (size_t)tx[0]+n>256) return TP_IO_ERROR;
    memcpy(rx,f->mem+(dev-0x50)*256+tx[0],n); return TP_OK;
}
int main(void) {
    fake f; memset(&f,0xff,sizeof(f)); f.fail_after=-1; f.writes=0; f.read_error=false;
    tp_i2c_io bus={&f,write_i2c,read_i2c}; tp_mb85rc16 chip;
    assert(tp_mb85rc16_init(&chip,&bus,1792)==TP_OK);
    tp_mb85rc16_log_region region;
    assert(tp_mb85rc16_log_init(&region,&chip,1760,64)==TP_INVALID); /* overlaps config */
    assert(tp_mb85rc16_log_init(&region,&chip,0,1792)==TP_OK);
    tp_event_log_io log_io=tp_mb85rc16_log_io(&region); tp_event_log log;
    assert(tp_event_log_open(&log,&log_io)==TP_OK && !log.has_records);
    tp_event event={.type=1,.value=42};
    assert(tp_event_log_append(&log,&event)==TP_OK && event.sequence==1);
    tp_event loaded; assert(tp_event_log_read(&log,1,&loaded)==TP_OK && loaded.value==42);
    for (unsigned i=2;i<=10;i++) { event.value=i; assert(tp_event_log_append(&log,&event)==TP_OK); }
    assert(tp_event_log_open(&log,&log_io)==TP_OK && log.latest_sequence==10);
    assert(tp_event_log_read(&log,9,&loaded)==TP_OK && loaded.value==9);
    tp_store_io io=tp_mb85rc16_store(&chip);
    tp_config cfg={.calibration={1,0,0,0.01f,10},.kp=1,.ki=0,.kd=0,.deadband_c=0,
        .output_max=1,.integral_max=10,.max_temperature_c=100,.period_seconds=3600,.slave_address=1};
    tp_config got; uint32_t seq; tp_config_source source;
    assert(tp_store_bootstrap(&io,&cfg,&got,&seq,&source)==TP_OK);
    assert(source==TP_CONFIG_DEFAULTED && seq==0 && got.period_seconds==3600 && f.writes==0);
    f.read_error=true; source=TP_CONFIG_FROM_STORAGE; seq=123;
    assert(tp_store_bootstrap(&io,&cfg,&got,&seq,&source)==TP_IO_ERROR);
    assert(source==TP_CONFIG_FROM_STORAGE && seq==123 && f.writes==0);
    f.read_error=false;
    assert(tp_store_load(&io,&got,&seq)==TP_INVALID);
    assert(tp_store_save(&io,&cfg)==TP_OK); assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==1);
    assert(tp_store_bootstrap(&io,&cfg,&got,&seq,&source)==TP_OK);
    assert(source==TP_CONFIG_FROM_STORAGE && seq==1 && got.period_seconds==3600);
    cfg.period_seconds=7200; f.writes=0; f.fail_after=4;
    assert(tp_store_save(&io,&cfg)==TP_IO_ERROR);
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==1 && got.period_seconds==3600);
    f.fail_after=-1; assert(tp_store_save(&io,&cfg)==TP_OK);
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==2 && got.period_seconds==7200);
    f.mem[1792+128+10] ^= 0x10;
    assert(tp_store_load(&io,&got,&seq)==TP_OK && seq==1 && got.period_seconds==3600);
    assert(tp_store_bootstrap(&io,&cfg,&got,&seq,&source)==TP_OK);
    assert(source==TP_CONFIG_FROM_STORAGE && seq==1 && got.period_seconds==3600);
    f.mem[1792+10] ^= 0x10;
    int writes_before=f.writes;
    assert(tp_store_bootstrap(&io,&cfg,&got,&seq,&source)==TP_OK);
    assert(source==TP_CONFIG_DEFAULTED && seq==0 && got.period_seconds==7200 && f.writes==writes_before);
    puts("FRAM config recovery checks passed"); return 0;
}
