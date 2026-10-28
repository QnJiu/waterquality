#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
static int safe_count;
static uint16_t check_faults; static int checks;
static tp_result preflight(void *ctx,uint16_t *faults) {
    (void)ctx; checks++; *faults=check_faults; return check_faults ? TP_IO_ERROR : TP_OK;
}
static tp_result valves(void *c, uint32_t v) { (void)c; (void)v; return TP_OK; }
static tp_result pump(void *c, int32_t s, uint32_t hz) { (void)c; (void)s; (void)hz; return TP_OK; }
static tp_result poll(void *c, bool *done) { (void)c; *done = true; return TP_OK; }
static tp_result temp(void *c, float *v) { (void)c; *v=20; return TP_OK; }
static tp_result heat(void *c, float v) { (void)c; (void)v; return TP_OK; }
static tp_result adc(void *c, uint8_t ch, float *v) { (void)c; (void)ch; *v=1; return TP_OK; }
static bool ok(void *c) { (void)c; return true; }
static void safe(void *c) { (void)c; safe_count++; }
int main(void) {
    tp_hal hal={NULL,valves,pump,poll,temp,heat,adc,ok,safe}; tp_controller c;
    tp_init(&c,&hal,NULL,NULL,100);
    tp_step recipe[]={ {.kind=TP_STEP_WAIT,.timeout_ms=1000,.arg.wait={1000}}, {.kind=TP_STEP_END} };
    tp_service s; assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    uint16_t config[]={1,0,1,0,0};
    assert(tp_service_write_holding(&s,1,5,config)==TP_OK);
    tp_service_tick(&s,999,false,0); assert(c.state==TP_IDLE);
    tp_service_tick(&s,1000,false,0); assert(c.state==TP_RUNNING);
    assert(tp_service_write_holding(&s,1,5,config)==TP_INVALID);
    uint16_t stop=2; assert(tp_service_write_holding(&s,0,1,&stop)==TP_OK);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP && safe_count==1);
    uint16_t status[7]; assert(tp_service_read_holding(&s,10,7,status)==TP_OK);
    assert(status[0]==TP_FAILED && status[1]==TP_FAULT_STOP && status[6]==0);
    uint16_t clear=3; assert(tp_service_write_holding(&s,0,1,&clear)==TP_OK);
    assert(c.state==TP_IDLE && safe_count==2);
    assert(tp_service_write_holding(&s,1,4,config)==TP_INVALID);
    assert(tp_service_write_holding(&s,0,1,&stop)==TP_OK);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP);
    tp_service_tick(&s,2000,false,0); assert(c.state==TP_FAILED);
    tp_init(&c,&hal,NULL,NULL,100);
    assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    assert(tp_service_attach_preflight(&s,NULL,preflight)==TP_OK);
    uint16_t selfcheck; assert(tp_service_read_holding(&s,19,1,&selfcheck)==TP_OK && selfcheck==TP_PREFLIGHT_NOT_RUN);
    uint16_t begin=1; check_faults=TP_PREFLIGHT_REAGENT; int before_safe=safe_count;
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_IO_ERROR);
    assert(c.state==TP_IDLE && safe_count==before_safe+1 && checks==1);
    assert(tp_service_read_holding(&s,19,1,&selfcheck)==TP_OK && selfcheck==TP_PREFLIGHT_REAGENT);
    check_faults=0; assert(tp_service_write_holding(&s,0,1,&begin)==TP_OK && checks==2);
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_BUSY && checks==2);
    tp_stop(&c); assert(tp_clear_fault(&c)==TP_OK);
    assert(tp_service_write_holding(&s,1,5,config)==TP_OK);
    check_faults=TP_PREFLIGHT_LEVEL; tp_service_tick(&s,1000,false,0);
    assert(c.state==TP_IDLE && checks==3 && s.last_start_result==TP_IO_ERROR);
    check_faults=0; tp_service_tick(&s,2000,false,0);
    assert(c.state==TP_RUNNING && checks==4);
    puts("service checks passed"); return 0;
}

