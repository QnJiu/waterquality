/* Exercise the real control task with simulated outputs and Modbus requests. */
#include "tp_freertos_runtime.h"
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
    {.kind=TP_STEP_WAIT,.timeout_ms=1000,.arg.wait={1000}},
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
static tp_result adc(void *ctx,uint8_t channel,float *value) { (void)ctx; (void)channel; *value=1; return TP_OK; }
static bool interlocks(void *ctx) { (void)ctx; return true; }
static void safe(void *ctx) { (void)valves(ctx,0); }
static tp_result send_response(void *ctx,unsigned port,const uint8_t *data,size_t len) {
    (void)ctx;
    if (port || len!=8) { fprintf(stderr,"response port=%u length=%u function=%02x code=%02x\n",port,(unsigned)len,data[1],data[2]); return TP_IO_ERROR; }
    return xQueueSend(responses,data,0)==pdPASS ? TP_OK : TP_IO_ERROR;
}
static uint16_t crc(const uint8_t *data,size_t len) {
    uint16_t c=0xffff;
    while(len--) { c^=*data++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1u)?0xa001u:0)); }
    return c;
}
static int command(uint8_t value) {
    uint8_t request[8]={1,6,0,0,0,value,0,0}, reply[8];
    uint16_t c=crc(request,6); request[6]=(uint8_t)c; request[7]=(uint8_t)(c>>8);
    return tp_runtime_submit(&runtime,0,request,sizeof(request))==TP_OK &&
        xQueueReceive(responses,reply,pdMS_TO_TICKS(1000))==pdPASS &&
        (memcmp(request,reply,sizeof(request))==0 || (fprintf(stderr,"reply %02x %02x %02x\n",reply[0],reply[1],reply[2]),0));
}
static void check(void *arg) {
    (void)arg;
    if (!command(1)) goto failed;
    /* Controlled mode pauses after the valve step. Allow up to one second. */
    for (unsigned i=0;i<100 && get_valves()!=1;i++) vTaskDelay(pdMS_TO_TICKS(10));
    if (get_valves()!=1) { fprintf(stderr,"valve before stop %u\n",(unsigned)get_valves()); goto failed; }
    if (!command(2)) goto failed;
    if (get_valves()!=0) { fprintf(stderr,"valve after stop %u\n",(unsigned)get_valves()); goto failed; }
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
    responses=xQueueCreate(4,8);
    if (!responses || tp_service_init(&service,&controller,recipe,3,0)!=TP_OK ||
        tp_runtime_init(&runtime,&service,&io,&address,1,10)!=TP_OK ||
        tp_runtime_start(&runtime,4096,2)!=TP_OK ||
        xTaskCreate(check,"check",4096,NULL,1,NULL)!=pdPASS) return 1;
    vTaskStartScheduler(); return 1;
}
