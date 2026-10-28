#include <assert.h>
#include "tp_f030_pn532_door.h"

typedef struct { bool unlocked; unsigned writes, aborts; } fake;
static tp_result write_start(void *ctx,uint8_t addr,const uint8_t *data,size_t n) {
    fake *f=ctx; assert(addr==TP_PN532_I2C_ADDRESS && data && n); f->writes++; return TP_OK;
}
static tp_result write_poll(void *ctx) { (void)ctx; return TP_BUSY; }
static tp_result read_start(void *ctx,uint8_t addr,size_t n,bool final) {
    (void)ctx; (void)addr; (void)n; (void)final; return TP_BUSY;
}
static tp_result read_continue(void *ctx,size_t n,bool final) {
    (void)ctx; (void)n; (void)final; return TP_OK;
}
static tp_result read_poll(void *ctx,uint8_t *data,size_t n) {
    (void)ctx; (void)data; (void)n; return TP_BUSY;
}
static void stop(void *ctx) { (void)ctx; }
static void abort_bus(void *ctx) { ((fake *)ctx)->aborts++; }
static tp_result lock(void *ctx,bool unlocked) { ((fake *)ctx)->unlocked=unlocked; return TP_OK; }
static tp_result request(void *ctx,uint32_t id,const uint8_t *card,size_t n) {
    (void)ctx; (void)id; (void)card; (void)n; return TP_OK;
}
int main(void) {
    fake f={.unlocked=true}; tp_f030_pn532_door d;
    tp_pn532_i2c_bus bus={&f,write_start,write_poll,read_start,read_continue,read_poll,stop,abort_bus};
    tp_f030_door_config cfg={.lock={&f,lock},.request=request,.command_timeout_ms=100,
        .scan_interval_ms=50,.scan_retries=1,.unlock_ms=20,.freshness_ms=200};
    assert(tp_f030_pn532_door_init(&d,&bus,&cfg)==TP_OK && !f.unlocked);
    assert(tp_door_service_tick(&d.service,0)==TP_BUSY && f.writes==1);
    assert(tp_door_service_tick(&d.service,99)==TP_BUSY);
    assert(tp_door_service_tick(&d.service,100)==TP_TIMEOUT);
    assert(d.service.door.state==TP_DOOR_FAULT && !f.unlocked);
    tp_f030_pn532_door_abort(&d);
    assert(f.aborts==1 && d.transport.state==TP_PN532_I2C_FAULT);
    tp_f030_pn532_door_abort(&d); assert(f.aborts==1);
    assert(tp_f030_pn532_door_recover(&d)==TP_OK && d.service.door.state==TP_DOOR_LOCKED);
    assert(d.transport.state==TP_PN532_I2C_IDLE);
    assert(tp_f030_pn532_door_recover(&d)==TP_INVALID);
    assert(tp_door_service_tick(&d.service,101)==TP_BUSY && f.writes==2);
    cfg.command_timeout_ms=0;
    assert(tp_f030_pn532_door_init(&d,&bus,&cfg)==TP_INVALID && f.aborts==2);
    return 0;
}
