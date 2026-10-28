#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static void set_float(uint16_t *p,float f) { uint32_t b; memcpy(&b,&f,4); p[0]=(uint16_t)(b>>16); p[1]=(uint16_t)b; }
static tp_config captured;
static tp_result save(void *ctx,const tp_config *cfg) { (void)ctx; captured=*cfg; return TP_OK; }
int main(void) {
    tp_controller c={0}; tp_service s; tp_step recipe[]={ {.kind=TP_STEP_END} };
    tp_config cfg={.calibration={1,0,0,0.01f,10},.kp=1,.ki=0,.kd=0,.deadband_c=0,
        .output_max=1,.integral_max=10,.max_temperature_c=100,.period_seconds=3600,.slave_address=1};
    assert(tp_service_init(&s,&c,recipe,1,0)==TP_OK);
    assert(tp_service_attach_config(&s,&cfg,NULL,save)==TP_OK && c.max_temperature_c==100);
    uint16_t words[27]; assert(tp_service_read_holding(&s,20,27,words)==TP_OK);
    words[24]=0; words[25]=7200; assert(tp_service_write_holding(&s,20,27,words)==TP_OK);
    words[26]=0; assert(tp_service_write_holding(&s,20,27,words)==TP_INVALID);
    assert(s.config.slave_address==1 && s.config.period_seconds==7200);
    uint16_t schedule[5]={1,0,10,0,0};
    assert(tp_service_write_holding(&s,1,5,schedule)==TP_OK);
    assert(s.config.period_seconds==10 && s.schedule.period_ms==10000);
    assert(tp_service_read_holding(&s,20,27,words)==TP_OK && words[25]==10);
    words[24]=32; words[25]=65535; /* exceeds the supported limit */
    assert(tp_service_write_holding(&s,20,27,words)==TP_INVALID);
    words[24]=0; words[25]=7200;
    assert(tp_service_write_holding(&s,20,27,words)==TP_OK);
    assert(s.schedule.period_ms==7200000);
    uint16_t command=4; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    assert(captured.period_seconds==7200 && s.save_status==2);
    assert(tp_service_write_holding(&s,1,5,schedule)==TP_INVALID);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_BUSY);
    tp_service_save_complete(&s,TP_IO_ERROR); assert(s.save_status==4);
    command=4; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_save_complete(&s,TP_OK); assert(s.save_status==3);
    uint16_t standards[18]; const float points[]={1,1,0,0,1,0.1f,0,5,0.01f};
    for (unsigned i=0;i<9;i++) set_float(standards+2*i,points[i]);
    assert(tp_service_write_holding(&s,70,18,standards)==TP_OK);
    assert(fabsf(s.config.calibration.slope-5)<0.0001f && c.calibration.slope==s.config.calibration.slope);
    set_float(standards+10,1); /* identical optical readings must leave configuration untouched */
    assert(tp_service_write_holding(&s,70,18,standards)==TP_INVALID);
    assert(fabsf(s.config.calibration.slope-5)<0.0001f);
    set_float(standards+10,0.1f); c.state=TP_RUNNING;
    assert(tp_service_write_holding(&s,70,18,standards)==TP_INVALID);
    c.state=TP_IDLE; command=4; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    assert(fabsf(captured.calibration.slope-5)<0.0001f);
    assert(tp_service_write_holding(&s,70,18,standards)==TP_INVALID);
    tp_service_save_complete(&s,TP_OK);
    puts("configuration service checks passed"); return 0;
}
