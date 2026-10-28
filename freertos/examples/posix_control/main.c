/* Exercise the real control task with simulated outputs and Modbus requests. */
#include "tp_freertos_runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tp_freertos_runtime runtime;
static tp_controller controller;
static tp_service service;
static QueueHandle_t responses;
static uint32_t valve_mask;
static const tp_step recipe[] = {
    {.kind=TP_STEP_VALVES,.arg.valves={1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_BLANK,0,1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,1,1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_TURBIDITY,2,1}},
    {.kind=TP_STEP_CALCULATE},
    {.kind=TP_STEP_END}
};
static tp_result valves(void *ctx,uint32_t mask) {
    (void)ctx; taskENTER_CRITICAL(); valve_mask=mask; taskEXIT_CRITICAL(); return TP_OK;
}
static uint32_t get_valves(void) {
    uint32_t mask; taskENTER_CRITICAL(); mask=valve_mask; taskEXIT_CRITICAL(); return mask;
}
static tp_result pump(void *ctx,int32_t steps,uint32_t hz) { (void)ctx; (void)steps; (void)hz; return TP_OK; }
static tp_result pump_poll(void *ctx,bool *done) { (void)ctx; *done=true; return TP_OK; }
static tp_result temperature(void *ctx,float *value) { (void)ctx; *value=20; return TP_OK; }
static tp_result heater(void *ctx,float value) { (void)ctx; (void)value; return TP_OK; }
static tp_result adc(void *ctx,uint8_t channel,float *value) {
    static const float samples[]={1.0f,0.5f,0.1f};
    (void)ctx; if(channel>=3) return TP_INVALID; *value=samples[channel]; return TP_OK;
}
static bool interlocks(void *ctx) { (void)ctx; return true; }
static void safe(void *ctx) { (void)valves(ctx,0); }
typedef struct { size_t length; uint8_t bytes[32]; } response;
static tp_result send_response(void *ctx,unsigned port,const uint8_t *data,size_t len) {
    (void)ctx;
    if (port || len>sizeof(((response *)0)->bytes)) return TP_IO_ERROR;
    response reply={.length=len}; memcpy(reply.bytes,data,len);
    return xQueueSend(responses,&reply,0)==pdPASS ? TP_OK : TP_IO_ERROR;
}
static uint16_t crc(const uint8_t *data,size_t len) {
    uint16_t c=0xffff;
    while(len--) { c^=*data++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1u)?0xa001u:0)); }
    return c;
}
static int exchange(uint8_t request[8],response *reply) {
    uint16_t c=crc(request,6); request[6]=(uint8_t)c; request[7]=(uint8_t)(c>>8);
    return tp_runtime_submit(&runtime,0,request,8)==TP_OK &&
        xQueueReceive(responses,reply,pdMS_TO_TICKS(1000))==pdPASS &&
        reply->length>=5 && crc(reply->bytes,reply->length-2)==
            (uint16_t)(reply->bytes[reply->length-2] | (uint16_t)reply->bytes[reply->length-1]<<8);
}
static int command(uint8_t value) {
    uint8_t request[8]={1,6,0,0,0,value,0,0}; response reply;
    return exchange(request,&reply) && reply.length==8 && !memcmp(request,reply.bytes,8);
}
/* Read one owner-task snapshot: state, fault, step, start result, result, valid,
 * save status, and whether controlled mode is waiting for the next command. */
static int status(uint16_t registers[9]) {
    uint8_t request[8]={1,3,0,10,0,9,0,0}; response reply;
    if (!exchange(request,&reply) || reply.length!=23 || reply.bytes[0]!=1 ||
        reply.bytes[1]!=3 || reply.bytes[2]!=18) return 0;
    for(unsigned i=0;i<9;i++) registers[i]=(uint16_t)((uint16_t)reply.bytes[3+2*i]<<8 | reply.bytes[4+2*i]);
    return 1;
}
static int wait_for(uint16_t state,uint16_t index,bool awaiting,uint16_t values[9]) {
    for(unsigned i=0;i<100;i++) {
        if (!status(values)) return 0;
        if (values[0]==state && values[2]==index && !!values[8]==awaiting) return 1;
        if (values[0]==TP_FAILED) return 0;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return 0;
}
static void check(void *arg) {
    (void)arg; uint16_t values[9];
    if (!command(1)) goto failed;
    /* Each completed step must wait for a separate advance command. */
    for(uint16_t next=1;next<=5;next++) {
        if (!wait_for(TP_RUNNING,next,true,values) || !command(5)) goto failed;
    }
    if (!wait_for(TP_COMPLETE,5,false,values) || values[1] || values[6]!=1 || get_valves()!=0) goto failed;
    uint32_t bits=(uint32_t)values[4]<<16 | values[5]; float result;
    memcpy(&result,&bits,sizeof(result));
    if (fabsf(result-0.30103f)>0.0001f) goto failed;
    /* A second run is stopped while awaiting the next controlled step. */
    if (!command(1) || !wait_for(TP_RUNNING,1,true,values) || get_valves()!=1 ||
        !command(2) || !status(values) || values[0]!=TP_FAILED ||
        values[1]!=TP_FAULT_STOP || values[6]!=0 || get_valves()!=0) goto failed;
    puts("TP_FREERTOS_CONTROL_SELFTEST PASS"); fflush(stdout); exit(0);
failed:
    fputs("TP_FREERTOS_CONTROL_SELFTEST FAIL\n",stderr); exit(1);
}
int main(void) {
    tp_hal hal={.set_valves=valves,.pump_start=pump,.pump_poll=pump_poll,
        .read_temperature=temperature,.set_heater=heater,.read_adc=adc,
        .interlocks_ok=interlocks,.safe_outputs=safe};
    tp_calibration calibration={1,0,0,0.01f,10};
    tp_pid pid={.output_max=1,.integral_max=20};
    const uint8_t address=1;
    tp_runtime_io io={.send=send_response};
    tp_init(&controller,&hal,&calibration,&pid,80);
    responses=xQueueCreate(4,sizeof(response));
    if (!responses || tp_service_init(&service,&controller,recipe,sizeof(recipe)/sizeof(recipe[0]),0)!=TP_OK ||
        tp_runtime_init(&runtime,&service,&io,&address,1,10)!=TP_OK ||
        tp_runtime_start(&runtime,4096,2)!=TP_OK ||
        xTaskCreate(check,"check",4096,NULL,1,NULL)!=pdPASS) return 1;
    vTaskStartScheduler(); return 1;
}
