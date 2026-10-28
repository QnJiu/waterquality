#include "tp_modbus.h"
#include "tp_service.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* This model and its recipe are for software demonstration only. */
typedef struct {
    uint32_t now, pump_done, valves;
    float temperature, heater;
    bool interlock, pump_active, sensor_error;
    unsigned safe_count, pump_count;
    uint32_t pump_valves[4];
} plant;
static tp_result valves(void *p,uint32_t mask) { ((plant *)p)->valves=mask; return TP_OK; }
static tp_result pump(void *p,int32_t steps,uint32_t hz) {
    plant *x=p;
    if(steps<=0 || !hz) return TP_INVALID;
    if(x->pump_count>=sizeof(x->pump_valves)/sizeof(x->pump_valves[0])) return TP_IO_ERROR;
    x->pump_valves[x->pump_count++]=x->valves;
    x->pump_done=x->now+(uint32_t)steps*1000u/hz; x->pump_active=true; return TP_OK;
}
static tp_result pump_poll(void *p,bool *done) {
    plant *x=p; *done=x->pump_active && (int32_t)(x->now-x->pump_done)>=0;
    if(*done) x->pump_active=false;
    return TP_OK;
}
static tp_result temperature(void *p,float *value) {
    *value=((plant *)p)->temperature; return TP_OK;
}
static tp_result heater(void *p,float value) { ((plant *)p)->heater=value; return TP_OK; }
static tp_result adc(void *p,uint8_t channel,float *value) {
    static const float samples[]={1.0f,0.5f,0.1f};
    if(((plant *)p)->sensor_error || channel>=3) return TP_IO_ERROR;
    *value=samples[channel]; return TP_OK;
}
static bool interlock(void *p) { return ((plant *)p)->interlock; }
static void safe(void *p) {
    plant *x=p; x->valves=0; x->heater=0; x->pump_active=false; x->safe_count++;
}
static uint16_t resources(void *p) { (void)p; return 0; }
static tp_result preflight(void *p,uint16_t *faults) {
    tp_preflight_io io={p,interlock,temperature,resources,80.0f};
    return tp_preflight_check(&io,faults);
}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t value=0xffff;
    while(n--) { value^=*p++; for(unsigned i=0;i<8;i++) value=(uint16_t)((value>>1)^((value&1u)?0xa001u:0)); }
    return value;
}
static tp_result command(tp_modbus *m,uint16_t value) {
    uint8_t q[8]={1,6,0,0,(uint8_t)(value>>8),(uint8_t)value}, response[8];
    uint16_t sum=crc(q,6); q[6]=(uint8_t)sum; q[7]=(uint8_t)(sum>>8);
    size_t n=0; tp_result r=tp_modbus_frame(m,q,sizeof(q),response,sizeof(response),&n);
    return r==TP_OK && n==8 && !memcmp(q,response,8) ? TP_OK : TP_IO_ERROR;
}
/* Illustrative valve bits: sample, reagent, rinse and drain. This is a
 * software exercise, not an analytical method or hardware configuration. */
enum { SAMPLE=1, REAGENT=2, RINSE=4, DRAIN=8 };
static const tp_step recipe[]={
    {.kind=TP_STEP_VALVES,.arg.valves={SAMPLE}},
    {.kind=TP_STEP_PUMP,.timeout_ms=3000,.arg.pump={100,100}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_BLANK,0,3}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_TURBIDITY,2,3}},
    {.kind=TP_STEP_VALVES,.arg.valves={REAGENT}},
    {.kind=TP_STEP_PUMP,.timeout_ms=3000,.arg.pump={100,100}},
    {.kind=TP_STEP_HEAT,.timeout_ms=20000,.arg.heat={30,0.5f}},
    {.kind=TP_STEP_HOLD,.timeout_ms=1000,.arg.wait={1000}},
    {.kind=TP_STEP_HEATER_OFF},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,1,3}},
    {.kind=TP_STEP_CALCULATE},
    {.kind=TP_STEP_VALVES,.arg.valves={RINSE}},
    {.kind=TP_STEP_PUMP,.timeout_ms=3000,.arg.pump={100,100}},
    {.kind=TP_STEP_VALVES,.arg.valves={DRAIN}},
    {.kind=TP_STEP_PUMP,.timeout_ms=3000,.arg.pump={100,100}},
    {.kind=TP_STEP_END}
};

int main(int argc,char **argv) {
    bool fault_interlock=argc==2 && !strcmp(argv[1],"interlock");
    bool fault_sensor=argc==2 && !strcmp(argv[1],"sensor");
    bool stop_rinse=argc==2 && !strcmp(argv[1],"stop-rinse");
    if(argc>2 || (argc==2 && !fault_interlock && !fault_sensor && !stop_rinse)) {
        fprintf(stderr,"usage: tp_simulator [interlock|sensor|stop-rinse]\n"); return 2;
    }
    plant x={.temperature=20,.interlock=true};
    tp_hal hal={&x,valves,pump,pump_poll,temperature,heater,adc,interlock,safe};
    tp_calibration calibration={1,0,0,0.01f,10};
    tp_pid pid={.kp=0.2f,.ki=0.02f,.output_max=1,.integral_max=20};
    tp_controller controller; tp_init(&controller,&hal,&calibration,&pid,80);
    tp_service service;
    if(tp_service_init(&service,&controller,recipe,sizeof(recipe)/sizeof(recipe[0]),0)!=TP_OK ||
       tp_service_attach_preflight(&service,&x,preflight)!=TP_OK) return 1;
    tp_modbus modbus={&service,1,tp_service_read_holding,tp_service_write_holding};
    if(command(&modbus,1)!=TP_OK) return 1;
    size_t last=(size_t)-1;
    for(x.now=0;x.now<30000 && controller.state==TP_RUNNING;x.now+=50) {
        x.temperature+=(x.heater*2.0f-(x.temperature-20)*0.02f)*0.05f;
        if(fault_interlock && x.pump_active) x.interlock=false;
        if(fault_sensor && recipe[controller.index].kind==TP_STEP_READ) x.sensor_error=true;
        if(stop_rinse && x.pump_active && x.valves==RINSE && command(&modbus,2)!=TP_OK) return 1;
        if(controller.awaiting_advance && command(&modbus,5)!=TP_OK) return 1;
        tp_service_tick(&service,x.now,false,0);
        if(controller.index!=last) {
            printf("t=%lu ms step=%lu temp=%.2f heater=%.2f\n",(unsigned long)x.now,
                   (unsigned long)controller.index,(double)x.temperature,(double)x.heater);
            last=controller.index;
        }
    }
    printf("state=%d fault=%d result_valid=%d result=%.4f safe=%u\n",controller.state,
           controller.fault,controller.result_valid,(double)controller.result_mg_l,x.safe_count);
    if(x.valves || x.heater!=0 || x.pump_active || !x.safe_count) return 1;
    if(fault_interlock) return controller.state==TP_FAILED && controller.fault==TP_FAULT_INTERLOCK ? 0 : 1;
    if(fault_sensor) return controller.state==TP_FAILED && controller.fault==TP_FAULT_SENSOR ? 0 : 1;
    if(stop_rinse) return controller.state==TP_FAILED && controller.fault==TP_FAULT_STOP &&
        x.pump_count==3 && x.pump_valves[2]==RINSE ? 0 : 1;
    if(x.pump_count!=4 || x.pump_valves[0]!=SAMPLE || x.pump_valves[1]!=REAGENT ||
       x.pump_valves[2]!=RINSE || x.pump_valves[3]!=DRAIN) return 1;
    return controller.state==TP_COMPLETE && controller.result_valid &&
        fabsf(controller.result_mg_l-0.30103f)<0.0001f ? 0 : 1;
}
