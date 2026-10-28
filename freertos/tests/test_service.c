#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void put_float(uint16_t *v,float f) { uint32_t bits; memcpy(&bits,&f,4); v[0]=(uint16_t)(bits>>16); v[1]=(uint16_t)bits; }
static float get_float(const uint16_t *v) { uint32_t bits=((uint32_t)v[0]<<16)|v[1]; float f; memcpy(&f,&bits,4); return f; }
static tp_config saved;
static tp_result save_config(void *ctx,const tp_config *cfg) { (void)ctx; saved=*cfg; return TP_OK; }
static int safe_count, recover_count;
static tp_result recovery_result;
static tp_result recover(void *ctx) { (void)ctx; recover_count++; return recovery_result; }
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
static uint32_t clock_request;
static uint8_t clock_state=1;
static tp_result set_clock(void *ctx,uint32_t seconds) {
    (void)ctx;
    if (clock_state==2) return TP_BUSY;
    if (seconds<946684800u || seconds>=4102444800u) return TP_INVALID;
    clock_request=seconds; clock_state=2; return TP_OK;
}
static uint8_t clock_status(void *ctx) { (void)ctx; return clock_state; }
int main(void) {
    tp_hal hal={NULL,valves,pump,poll,temp,heat,adc,ok,safe}; tp_controller c;
    tp_init(&c,&hal,NULL,NULL,100);
    tp_step recipe[]={ {.kind=TP_STEP_WAIT,.timeout_ms=1000,.arg.wait={1000}}, {.kind=TP_STEP_END} };
    tp_service s; assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    uint16_t version[3];
    assert(tp_service_read_holding(&s,119,3,version)==TP_OK);
    assert(version[0]==0 && version[1]==0 && version[2]==0);
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
    assert(tp_service_attach_adc_recovery(&s,NULL,recover)==TP_OK);
    uint16_t recovery=8; int safe_before_recovery=safe_count;
    recovery_result=TP_IO_ERROR;
    assert(tp_service_write_holding(&s,0,1,&recovery)==TP_IO_ERROR);
    assert(recover_count==1 && safe_count==safe_before_recovery+1);
    recovery_result=TP_OK;
    assert(tp_service_write_holding(&s,0,1,&recovery)==TP_OK && recover_count==2);
    uint16_t selfcheck; assert(tp_service_read_holding(&s,19,1,&selfcheck)==TP_OK && selfcheck==TP_PREFLIGHT_NOT_RUN);
    uint16_t begin=1; check_faults=TP_PREFLIGHT_REAGENT; int before_safe=safe_count;
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_IO_ERROR);
    assert(c.state==TP_IDLE && safe_count==before_safe+1 && checks==1);
    assert(tp_service_read_holding(&s,19,1,&selfcheck)==TP_OK && selfcheck==TP_PREFLIGHT_REAGENT);
    check_faults=0; assert(tp_service_write_holding(&s,0,1,&begin)==TP_OK && checks==2);
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_BUSY && checks==2);
    assert(tp_service_write_holding(&s,0,1,&recovery)==TP_BUSY && recover_count==2);
    tp_stop(&c); assert(tp_clear_fault(&c)==TP_OK);
    assert(tp_service_write_holding(&s,1,5,config)==TP_OK);
    check_faults=TP_PREFLIGHT_LEVEL; tp_service_tick(&s,1000,false,0);
    assert(c.state==TP_IDLE && checks==3 && s.last_start_result==TP_IO_ERROR);
    check_faults=0; tp_service_tick(&s,2000,false,0);
    assert(c.state==TP_RUNNING && checks==4);
    before_safe=safe_count;
    assert(tp_service_enter_maintenance(&s)==TP_OK);
    assert(s.maintenance && c.state==TP_FAILED && c.fault==TP_FAULT_STOP && safe_count==before_safe+1);
    assert(tp_service_enter_maintenance(&s)==TP_OK && safe_count==before_safe+1);
    assert(tp_service_write_holding(&s,0,1,&recovery)==TP_BUSY && recover_count==2);
    assert(tp_service_write_holding(&s,0,1,&clear)==TP_OK);
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_BUSY);
    uint16_t advance=5; assert(tp_service_write_holding(&s,0,1,&advance)==TP_BUSY);
    tp_service_tick(&s,5000,false,0); assert(c.state==TP_IDLE && s.schedule.last_period_ms==5000);
    uint16_t daily[]={TP_MODE_DAILY,0,1,1,0};
    assert(tp_service_write_holding(&s,1,5,daily)==TP_OK);
    tp_service_tick(&s,6000,true,60); assert(c.state==TP_IDLE && s.schedule.daily_fired);
    tp_init(&c,&hal,NULL,NULL,100);
    assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    before_safe=safe_count; assert(tp_service_enter_maintenance(&s)==TP_OK);
    assert(c.state==TP_IDLE && c.fault==TP_FAULT_NONE && safe_count==before_safe+1);
    assert(tp_service_enter_maintenance(NULL)==TP_INVALID);
    tp_config cfg={.calibration={1,0,0,0.01f,10},.kp=1,.output_max=1,.integral_max=10,
                   .max_temperature_c=100,.period_seconds=3600,.slave_address=1};
    assert(tp_service_attach_config(&s,&cfg,NULL,save_config)==TP_OK);
    uint16_t persisted[]={TP_MODE_DAILY,0,120,60,(uint16_t)-300};
    assert(tp_service_write_holding(&s,1,5,persisted)==TP_OK);
    uint16_t save=4; assert(tp_service_write_holding(&s,0,1,&save)==TP_OK);
    assert(saved.mode==TP_MODE_DAILY && saved.period_seconds==120 && saved.daily_minute==60 && saved.utc_offset_minutes==-300);
    tp_service_save_complete(&s,TP_OK);
    tp_init(&c,&hal,NULL,NULL,100); assert(tp_service_init(&s,&c,recipe,2,5000)==TP_OK);
    assert(tp_service_attach_config(&s,&saved,NULL,save_config)==TP_OK);
    uint16_t restored[5]; assert(tp_service_read_holding(&s,1,5,restored)==TP_OK);
    for(unsigned i=0;i<5;i++) assert(restored[i]==persisted[i]);
    tp_service_tick(&s,6000,false,21600); assert(c.state==TP_IDLE);
    tp_service_tick(&s,7000,true,21600); assert(c.state==TP_RUNNING);
    tp_init(&c,&hal,NULL,NULL,100); assert(tp_service_init(&s,&c,recipe,2,5000)==TP_OK);
    saved.mode=TP_MODE_PERIODIC; saved.period_seconds=2;
    assert(tp_service_attach_config(&s,&saved,NULL,save_config)==TP_OK);
    tp_service_tick(&s,6999,false,0); assert(c.state==TP_IDLE);
    tp_service_tick(&s,7000,false,0); assert(c.state==TP_RUNNING);
    tp_init(&c,&hal,NULL,NULL,100); assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    assert(tp_service_attach_config(&s,&cfg,NULL,save_config)==TP_OK);
    uint16_t check[10], check_result[3];
    put_float(check,1); put_float(check+2,0.1f); put_float(check+4,0);
    put_float(check+6,1); put_float(check+8,0.01f);
    assert(tp_service_write_holding(&s,92,9,check)==TP_INVALID);
    assert(tp_service_write_holding(&s,92,10,check)==TP_OK);
    assert(tp_service_read_holding(&s,102,3,check_result)==TP_OK);
    assert(check_result[0]==1 && fabsf(get_float(check_result+1)-1)<0.0001f);
    put_float(check+6,2);
    assert(tp_service_write_holding(&s,92,10,check)==TP_OK);
    assert(tp_service_read_holding(&s,102,3,check_result)==TP_OK && check_result[0]==2);
    put_float(check+8,-1);
    assert(tp_service_write_holding(&s,92,10,check)==TP_INVALID);
    assert(tp_service_read_holding(&s,102,3,check_result)==TP_OK && check_result[0]==0);
    put_float(check+8,0.01f);
    assert(tp_service_write_holding(&s,92,10,check)==TP_OK);
    assert(tp_service_attach_config(&s,&cfg,NULL,save_config)==TP_OK);
    assert(tp_service_read_holding(&s,102,3,check_result)==TP_OK && check_result[0]==0);
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_OK);
    assert(tp_service_write_holding(&s,92,10,check)==TP_BUSY);
    /* Standalone diagnostics never start or clear a fault. */
    uint16_t diagnose=10;
    tp_init(&c,&hal,NULL,NULL,100);
    assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    assert(tp_service_write_holding(&s,0,1,&diagnose)==TP_INVALID);
    assert(tp_service_attach_preflight(&s,NULL,preflight)==TP_OK);
    check_faults=TP_PREFLIGHT_LEVEL;
    assert(tp_service_write_holding(&s,0,1,&diagnose)==TP_IO_ERROR);
    assert(c.state==TP_IDLE && s.preflight_faults==TP_PREFLIGHT_LEVEL);
    tp_stop(&c); check_faults=0;
    assert(tp_service_write_holding(&s,0,1,&diagnose)==TP_OK);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP && s.preflight_faults==0);
    check_faults=TP_PREFLIGHT_OVERTEMPERATURE;
    assert(tp_service_write_holding(&s,0,1,&clear)==TP_IO_ERROR);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP && s.preflight_faults==TP_PREFLIGHT_OVERTEMPERATURE);
    check_faults=0;
    assert(tp_service_write_holding(&s,0,1,&clear)==TP_OK && c.state==TP_IDLE);
    assert(tp_service_write_holding(&s,0,1,&clear)==TP_INVALID);
    assert(tp_service_enter_maintenance(&s)==TP_OK);
    assert(tp_service_write_holding(&s,0,1,&diagnose)==TP_BUSY);
    tp_init(&c,&hal,NULL,NULL,100);
    assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    uint16_t time_words[2]={0x6553,0xf100}, clock_word=99; /* 1700000000 UTC */
    assert(tp_service_read_holding(&s,122,1,&clock_word)==TP_OK && clock_word==0);
    assert(tp_service_write_holding(&s,123,2,time_words)==TP_INVALID);
    uint16_t time_snapshot[3];
    tp_service_tick(&s,1,true,1700000000LL);
    assert(tp_service_read_holding(&s,125,3,time_snapshot)==TP_OK && time_snapshot[0]==1 &&
           time_snapshot[1]==0x6553 && time_snapshot[2]==0xf100);
    tp_service_tick(&s,2,false,1700000000LL);
    assert(tp_service_read_holding(&s,125,3,time_snapshot)==TP_OK && !time_snapshot[0] &&
           !time_snapshot[1] && !time_snapshot[2]);
    assert(tp_service_attach_clock(&s,NULL,set_clock,clock_status)==TP_OK);
    assert(tp_service_write_holding(&s,123,1,time_words)==TP_INVALID);
    assert(tp_service_write_holding(&s,123,2,time_words)==TP_OK && clock_request==1700000000u);
    assert(tp_service_read_holding(&s,122,1,&clock_word)==TP_OK && clock_word==2);
    uint16_t clock_registers[6];
    assert(tp_service_read_holding(&s,122,6,clock_registers)==TP_OK &&
           clock_registers[0]==2 && !clock_registers[1] && !clock_registers[2] &&
           !clock_registers[3] && !clock_registers[4] && !clock_registers[5]);
    tp_service_tick(&s,3,true,1700000000LL);
    assert(tp_service_read_holding(&s,125,3,time_snapshot)==TP_OK && time_snapshot[0]==0);
    assert(tp_service_write_holding(&s,123,2,time_words)==TP_BUSY);
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_BUSY);
    clock_state=3;
    assert(tp_service_write_holding(&s,0,1,&begin)==TP_OK);
    assert(tp_service_write_holding(&s,123,2,time_words)==TP_BUSY);
    /* An old RTC value during a queued clock change must not consume a daily trigger. */
    tp_init(&c,&hal,NULL,NULL,100);
    assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    clock_state=1;
    assert(tp_service_attach_clock(&s,NULL,set_clock,clock_status)==TP_OK);
    uint16_t daily_clock[]={TP_MODE_DAILY,0,1,(1700000000u%86400u)/60u,0};
    assert(tp_service_write_holding(&s,1,5,daily_clock)==TP_OK);
    assert(tp_service_write_holding(&s,123,2,time_words)==TP_OK);
    tp_service_tick(&s,1,true,1700000000LL);
    assert(c.state==TP_IDLE && !s.schedule.daily_fired);
    clock_state=3;
    tp_service_tick(&s,2,true,1700000000LL);
    assert(c.state==TP_RUNNING && s.schedule.daily_fired);
    puts("service checks passed"); return 0;
}

