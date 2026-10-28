#include "tp_service.h"
#include "tp_modbus.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static void set_float(uint16_t *p,float f) { uint32_t b; memcpy(&b,&f,4); p[0]=(uint16_t)(b>>16); p[1]=(uint16_t)b; }
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static void append_crc(uint8_t *p,size_t n) { uint16_t c=crc(p,n); p[n]=(uint8_t)c; p[n+1]=(uint8_t)(c>>8); }
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
    /* Exercise the complete RTU frame path, including readback and rejection. */
    tp_modbus port={&s,1,tp_service_read_holding,tp_service_write_holding};
    uint8_t request[45]={1,16,0,70,0,18,36}, response[32]; size_t n;
    for (unsigned i=0;i<18;i++) { request[7+2*i]=(uint8_t)(standards[i]>>8); request[8+2*i]=(uint8_t)standards[i]; }
    append_crc(request,43);
    assert(tp_modbus_frame(&port,request,sizeof(request),response,sizeof(response),&n)==TP_OK);
    assert(n==8 && response[1]==16 && response[3]==70 && response[5]==18 && crc(response,n)==0);
    uint8_t readback[]={1,3,0,20,0,2,0,0}; append_crc(readback,6);
    assert(tp_modbus_frame(&port,readback,sizeof(readback),response,sizeof(response),&n)==TP_OK);
    assert(n==9 && response[1]==3 && response[2]==4 && crc(response,n)==0);
    uint32_t bits=((uint32_t)response[3]<<24)|((uint32_t)response[4]<<16)|((uint32_t)response[5]<<8)|response[6];
    float slope; memcpy(&slope,&bits,sizeof(slope)); assert(fabsf(slope-5)<0.0001f);
    /* Make the high point identical to the low point and verify exception and unchanged configuration. */
    memcpy(request+7+10*2,request+7+2*2,4); append_crc(request,43);
    assert(tp_modbus_frame(&port,request,sizeof(request),response,sizeof(response),&n)==TP_OK);
    assert(n==5 && response[1]==0x90 && response[2]==2 && crc(response,n)==0);
    assert(fabsf(s.config.calibration.slope-5)<0.0001f);
    request[44]^=1;
    assert(tp_modbus_frame(&port,request,sizeof(request),response,sizeof(response),&n)==TP_INVALID && n==0);
    puts("configuration service checks passed"); return 0;
}
