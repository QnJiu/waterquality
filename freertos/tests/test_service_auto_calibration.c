#include "tp_service.h"
#include "tp_modbus.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct { unsigned reads,checks,safe,saves,recoveries; bool fail_second; tp_config saved; } fake;
static tp_result valves(void *p,uint32_t m) {(void)p;(void)m;return TP_OK;}
static tp_result pump(void *p,int32_t n,uint32_t hz) {(void)p;(void)n;(void)hz;return TP_OK;}
static tp_result poll(void *p,bool *done) {(void)p;*done=true;return TP_OK;}
static tp_result temp(void *p,float *v) {(void)p;*v=20;return TP_OK;}
static tp_result heater(void *p,float v) {(void)p;(void)v;return TP_OK;}
static tp_result adc(void *p,uint8_t ch,float *v) {
    fake *f=p; (void)ch; const float samples[]={1,1,0,1,0.1f,0};
    assert(f->reads<6); *v=samples[f->reads++]; return TP_OK;
}
static bool interlock(void *p) {(void)p;return true;}
static void safe(void *p) {((fake *)p)->safe++;}
static tp_result preflight(void *p,uint16_t *faults) {
    fake *f=p; f->checks++; *faults=(f->fail_second && f->checks==2) ? TP_PREFLIGHT_REAGENT : 0; return TP_OK;
}
static tp_result recover(void *p) { ((fake *)p)->recoveries++; return TP_OK; }
static tp_result save(void *p,const tp_config *cfg) {fake *f=p; f->saved=*cfg; f->saves++; return TP_OK;}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) {c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0));} return c;
}
static void exchange(tp_modbus *m,uint8_t fn,uint16_t addr,uint16_t val,uint8_t *response) {
    uint8_t q[8]={1,fn,(uint8_t)(addr>>8),(uint8_t)addr,(uint8_t)(val>>8),(uint8_t)val,0,0};
    uint16_t c=crc(q,6); q[6]=(uint8_t)c; q[7]=(uint8_t)(c>>8); size_t n=0;
    assert(tp_modbus_frame(m,q,8,response,32,&n)==TP_OK && n>=5 && crc(response,n)==0);
}
static const tp_step recipe[]={
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_BLANK,.samples=1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_REACTED,.samples=1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_TURBIDITY,.samples=1}},
    {.kind=TP_STEP_END}
};
int main(void) {
    fake f={0}; tp_hal hal={&f,valves,pump,poll,temp,heater,adc,interlock,safe};
    tp_calibration base={1,0,0,0.001f,10}; tp_controller c; tp_init(&c,&hal,&base,NULL,100);
    tp_service s; assert(tp_service_init(&s,&c,recipe,4,0)==TP_OK);
    tp_config cfg={.calibration={1,0,0,0.001f,10},.kp=1,.output_max=1,.integral_max=10,
                   .max_temperature_c=100,.period_seconds=1,.slave_address=1};
    assert(tp_service_attach_config(&s,&cfg,&f,save)==TP_OK);
    assert(tp_service_attach_preflight(&s,&f,preflight)==TP_OK);
    assert(tp_service_attach_adc_recovery(&s,&f,recover)==TP_OK);
    assert(tp_service_attach_auto_calibration(&s,recipe,4,recipe,4,0,5,0.01f)==TP_OK);
    tp_modbus m={&s,1,tp_service_read_holding,tp_service_write_holding}; uint8_t r[32];
    exchange(&m,6,0,6,r); assert(r[1]==6 && s.auto_cal.state==TP_AUTO_CAL_LOW);
    exchange(&m,6,0,1,r); assert(r[1]==0x86); /* normal start is excluded */
    exchange(&m,6,0,4,r); assert(r[1]==0x86); /* cannot save while measuring */
    for(unsigned i=0;i<8;i++) tp_service_tick(&s,i*50,false,0);
    assert(s.auto_cal.state==TP_AUTO_CAL_COMPLETE && f.reads==6 && f.checks==2 && f.safe==2);
    exchange(&m,3,90,2,r); assert(r[1]==3 && r[4]==4 && r[6]==0);
    assert(c.calibration.slope==1 && f.saves==0);
    exchange(&m,6,0,7,r); assert(r[1]==6 && fabsf(c.calibration.slope-5)<0.0001f);
    assert(f.saves==0 && s.save_status==1);
    exchange(&m,6,0,4,r); assert(r[1]==6 && f.saves==1 && fabsf(f.saved.calibration.slope-5)<0.0001f);
    tp_service_save_complete(&s,TP_OK);
    f.reads=0; f.checks=0; f.fail_second=true;
    exchange(&m,6,0,6,r); assert(r[1]==6);
    for(unsigned i=0;i<4;i++) tp_service_tick(&s,500+i*50,false,0);
    assert(s.auto_cal.state==TP_AUTO_CAL_FAILED && s.preflight_faults==TP_PREFLIGHT_REAGENT);
    assert(f.reads==3 && c.calibration.slope==5);
    exchange(&m,6,0,7,r); assert(r[1]==0x86 && c.calibration.slope==5);
    f.reads=0; f.checks=0; f.fail_second=false;
    exchange(&m,6,0,6,r); assert(r[1]==6);
    exchange(&m,6,0,2,r); assert(r[1]==6 && s.auto_cal.state==TP_AUTO_CAL_CANCELLED && c.fault==TP_FAULT_STOP);
    uint16_t clear=3; assert(tp_service_write_holding(&s,0,1,&clear)==TP_OK);
    uint16_t periodic[]={TP_MODE_PERIODIC,0,1,0,0};
    assert(tp_service_write_holding(&s,1,5,periodic)==TP_OK);
    f.reads=0; f.checks=0;
    exchange(&m,6,0,6,r); assert(r[1]==6);
    for(unsigned i=0;i<8;i++) tp_service_tick(&s,650+i*250,false,0);
    assert(s.auto_cal.state==TP_AUTO_CAL_COMPLETE && f.checks==2 && f.reads==6);
    uint16_t controlled[]={TP_MODE_CONTROLLED,0,1,0,0};
    assert(tp_service_write_holding(&s,1,5,controlled)==TP_OK);
    assert(tp_service_attach_auto_standard(&s,recipe,4,0,0.01f)==TP_OK);
    /* Model the boundary where the controller is complete but the owning
     * automatic operation has not yet published its final state. */
    uint16_t words[27], points[18]={0}, check[10]={0};
    assert(tp_service_read_holding(&s,20,27,words)==TP_OK);
    for (unsigned kind=0;kind<2;kind++) {
        c.state=TP_COMPLETE;
        s.auto_cal.state=kind ? TP_AUTO_CAL_COMPLETE : TP_AUTO_CAL_LOW;
        s.auto_standard.state=kind ? TP_STANDARD_RUNNING : TP_STANDARD_IDLE;
        unsigned saves=f.saves, safes=f.safe;
        uint16_t save_cmd=4, recovery_cmd=8, cal_cmd=6, apply_cmd=7;
        assert(tp_service_attach_config(&s,&cfg,&f,save)==TP_BUSY);
        assert(tp_service_write_holding(&s,0,1,&save_cmd)==TP_BUSY);
        assert(tp_service_write_holding(&s,0,1,&recovery_cmd)==TP_BUSY);
        assert(tp_service_write_holding(&s,0,1,&cal_cmd)==TP_BUSY);
        assert(tp_service_write_holding(&s,0,1,&apply_cmd)==TP_BUSY);
        assert(tp_service_write_holding(&s,20,27,words)!=TP_OK);
        assert(tp_service_write_holding(&s,70,18,points)!=TP_OK);
        assert(tp_service_write_holding(&s,92,10,check)==TP_BUSY);
        assert(tp_service_write_holding(&s,1,5,controlled)==TP_BUSY);
        assert(f.saves==saves && f.safe==safes && f.recoveries==0);
    }
    s.auto_cal.state=TP_AUTO_CAL_COMPLETE; s.auto_standard.state=TP_STANDARD_IDLE;
    f.reads=0; f.checks=0;
    exchange(&m,6,0,9,r); assert(r[1]==6 && s.auto_standard.state==TP_STANDARD_RUNNING);
    exchange(&m,6,0,6,r); assert(r[1]==0x86);
    exchange(&m,6,0,1,r); assert(r[1]==0x86);
    for(unsigned i=0;i<4;i++) tp_service_tick(&s,2500+i*50,false,0);
    exchange(&m,3,105,5,r);
    assert(r[1]==3 && r[4]==3 && r[6]==0 && r[8]==1 && f.reads==3 && f.checks==1);
    assert(c.calibration.slope==5);
    f.reads=0; exchange(&m,6,0,9,r); assert(r[1]==6);
    exchange(&m,6,0,2,r); assert(r[1]==6 && s.auto_standard.state==TP_STANDARD_CANCELLED);
    assert(c.fault==TP_FAULT_STOP);
    assert(tp_service_write_holding(&s,0,1,&clear)==TP_OK);
    exchange(&m,6,0,9,r); assert(r[1]==6);
    assert(tp_service_enter_maintenance(&s)==TP_OK);
    assert(s.auto_standard.state==TP_STANDARD_CANCELLED && c.fault==TP_FAULT_STOP);
    assert(tp_service_attach_config(&s,&cfg,&f,save)==TP_BUSY);
    /* At a scheduled boundary, the automatic workflow still owns a completed
     * controller until it publishes its result. Do not attempt a normal start. */
    for (unsigned kind=0; kind<2; kind++) {
        fake local={0}; tp_hal local_hal={&local,valves,pump,poll,temp,heater,adc,interlock,safe};
        tp_controller controller; tp_init(&controller,&local_hal,&base,NULL,100);
        tp_service service; assert(tp_service_init(&service,&controller,recipe,4,0)==TP_OK);
        assert(tp_service_attach_config(&service,&cfg,&local,save)==TP_OK);
        assert(tp_service_attach_preflight(&service,&local,preflight)==TP_OK);
        assert(tp_service_attach_auto_calibration(&service,recipe,4,recipe,4,0,5,0.01f)==TP_OK);
        assert(tp_service_attach_auto_standard(&service,recipe,4,0,0.01f)==TP_OK);
        assert(tp_service_write_holding(&service,1,5,periodic)==TP_OK);
        uint16_t command=kind ? 9 : 6;
        assert(tp_service_write_holding(&service,0,1,&command)==TP_OK);
        /* Advance the recipe to its final controller state before the service
         * consumes it, as may happen at an ownership boundary. */
        for(unsigned i=0;i<4;i++) tp_tick(&controller,i*50);
        assert(controller.state==TP_COMPLETE && local.reads==3);
        service.last_start_result=TP_IO_ERROR;
        tp_service_tick(&service,1000,false,0);
        assert(service.last_start_result==TP_IO_ERROR && service.schedule.last_period_ms==1000);
        if (kind) {
            assert(service.auto_standard.state==TP_STANDARD_COMPLETE && controller.state==TP_COMPLETE);
        } else {
            assert(service.auto_cal.state==TP_AUTO_CAL_HIGH && controller.state==TP_RUNNING);
        }
    }
    puts("service automatic calibration checks passed"); return 0;
}
