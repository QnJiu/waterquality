/* Exercise the real control task with simulated outputs and Modbus requests. */
#include "tp_freertos_runtime.h"
#include "tp_freertos_event_log.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tp_freertos_runtime runtime;
static tp_controller controller;
static tp_service service;
static QueueHandle_t responses;
static uint32_t valve_mask;
static uint8_t slots[2][128];
static uint8_t log_bytes[256];
static tp_freertos_event_log events;
static tp_result log_read(void *ctx,uint32_t offset,void *data,size_t len) {
    (void)ctx; if(offset>sizeof(log_bytes) || len>sizeof(log_bytes)-offset) return TP_INVALID;
    memcpy(data,log_bytes+offset,len); return TP_OK;
}
static tp_result log_write(void *ctx,uint32_t offset,const void *data,size_t len) {
    (void)ctx; if(offset>sizeof(log_bytes) || len>sizeof(log_bytes)-offset) return TP_INVALID;
    memcpy(log_bytes+offset,data,len); return TP_OK;
}
static void record_event(void *ctx,const tp_event *event) {
    (void)ctx; (void)tp_freertos_event_log_submit(&events,event);
}
static tp_result request_log(void *ctx,bool latest,uint32_t sequence) {
    (void)ctx; return tp_freertos_event_log_query(&events,latest,sequence);
}
static bool poll_log(void *ctx,tp_result *result,tp_event *event) {
    (void)ctx; return tp_freertos_event_log_poll(&events,result,event);
}
static atomic_bool fail_erase;
static tp_result store_read(void *ctx,unsigned slot,uint32_t offset,void *data,size_t len) {
    (void)ctx; if (slot>=2 || offset>128 || len>128-offset) return TP_INVALID;
    memcpy(data,slots[slot]+offset,len); return TP_OK;
}
static tp_result store_write(void *ctx,unsigned slot,uint32_t offset,const void *data,size_t len) {
    (void)ctx; if (slot>=2 || offset>128 || len>128-offset) return TP_INVALID;
    memcpy(slots[slot]+offset,data,len); return TP_OK;
}
static tp_result store_erase(void *ctx,unsigned slot) {
    (void)ctx; if(slot>=2) return TP_INVALID;
    if(atomic_load(&fail_erase)) return TP_IO_ERROR;
    memset(slots[slot],0xff,sizeof(slots[slot])); return TP_OK;
}
static const tp_store_io store={NULL,store_read,store_erase,store_write};
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
typedef struct { unsigned port; size_t length; uint8_t bytes[32]; } response;
static tp_result send_response(void *ctx,unsigned port,const uint8_t *data,size_t len) {
    (void)ctx;
    if (port>=2 || len>sizeof(((response *)0)->bytes)) return TP_IO_ERROR;
    response reply={.port=port,.length=len}; memcpy(reply.bytes,data,len);
    return xQueueSend(responses,&reply,0)==pdPASS ? TP_OK : TP_IO_ERROR;
}
static uint16_t crc(const uint8_t *data,size_t len) {
    uint16_t c=0xffff;
    while(len--) { c^=*data++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1u)?0xa001u:0)); }
    return c;
}
static int exchange(uint8_t *request,size_t length,response *reply) {
    uint16_t c=crc(request,length-2); request[length-2]=(uint8_t)c; request[length-1]=(uint8_t)(c>>8);
    return tp_runtime_submit(&runtime,0,request,length)==TP_OK &&
        xQueueReceive(responses,reply,pdMS_TO_TICKS(1000))==pdPASS &&
        reply->port==0 && reply->length>=5 && crc(reply->bytes,reply->length-2)==
            (uint16_t)(reply->bytes[reply->length-2] | (uint16_t)reply->bytes[reply->length-1]<<8);
}
static int command(uint8_t value) {
    uint8_t request[8]={1,6,0,0,0,value,0,0}; response reply;
    return exchange(request,sizeof(request),&reply) && reply.length==8 && !memcmp(request,reply.bytes,8);
}
/* Read one owner-task snapshot: state, fault, step, start result, result, valid,
 * save status, and whether controlled mode is waiting for the next command. */
static int status(uint16_t registers[9]) {
    uint8_t request[8]={1,3,0,10,0,9,0,0}; response reply;
    if (!exchange(request,sizeof(request),&reply) || reply.length!=23 || reply.bytes[0]!=1 ||
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
static int set_period(uint16_t seconds) {
    /* Set controlled mode and the five schedule registers in one FC16 request. */
    uint8_t request[19]={1,16,0,1,0,5,10,0,0,0,0,0,0,0,0,0,0,0,0};
    request[11]=(uint8_t)(seconds>>8); request[12]=(uint8_t)seconds;
    response reply;
    return exchange(request,sizeof(request),&reply) && reply.length==8 &&
        reply.bytes[0]==1 && reply.bytes[1]==16 && reply.bytes[3]==1 && reply.bytes[5]==5;
}
static int wait_save(uint16_t expected) {
    uint16_t values[9];
    for(unsigned i=0;i<100;i++) {
        if(!status(values)) return 0;
        if(values[7]==expected) return 1;
        if(values[7]==4 && expected!=4) return 0;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return 0;
}
static int query_last_result(float expected) {
    uint8_t request[15]={1,16,0,50,0,3,6,0,0,0,0,0,0,0,0}; response reply;
    if (!exchange(request,sizeof(request),&reply) || reply.length!=8 || reply.bytes[1]!=16) return 0;
    for(unsigned i=0;i<100;i++) {
        uint8_t read[8]={1,3,0,53,0,12,0,0};
        if (!exchange(read,sizeof(read),&reply) || reply.length!=29 || reply.bytes[1]!=3) return 0;
        uint16_t status=(uint16_t)((uint16_t)reply.bytes[3]<<8 | reply.bytes[4]);
        if(status==3) {
            uint16_t type=(uint16_t)((uint16_t)reply.bytes[9]<<8 | reply.bytes[10]);
            uint16_t detail=(uint16_t)((uint16_t)reply.bytes[11]<<8 | reply.bytes[12]);
            uint32_t bits=(uint32_t)reply.bytes[23]<<24 | (uint32_t)reply.bytes[24]<<16 |
                          (uint32_t)reply.bytes[25]<<8 | reply.bytes[26];
            float value; memcpy(&value,&bits,sizeof(value));
            return type==TP_EVENT_COMPLETED && (detail&1u) && fabsf(value-expected)<0.0001f;
        }
        if(status!=2) return 0;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return 0;
}
static int check_two_ports(uint16_t state) {
    uint8_t requests[2][8]={{1,3,0,10,0,1,0,0},{2,3,0,20,0,1,0,0}};
    for (unsigned port=0;port<2;port++) {
        uint16_t c=crc(requests[port],6);
        requests[port][6]=(uint8_t)c; requests[port][7]=(uint8_t)(c>>8);
        if (tp_runtime_submit(&runtime,port,requests[port],8)!=TP_OK) return 0;
    }
    unsigned seen=0;
    for (unsigned i=0;i<2;i++) {
        response reply;
        if (xQueueReceive(responses,&reply,pdMS_TO_TICKS(1000))!=pdPASS ||
            reply.port>=2 || (seen & (1u<<reply.port)) || reply.length!=7 ||
            reply.bytes[0]!=reply.port+1 || reply.bytes[1]!=3 || reply.bytes[2]!=2 ||
            reply.bytes[3]!=(reply.port==0 ? (state >> 8) : 0x3f) ||
            reply.bytes[4]!=(reply.port==0 ? (state & 0xff) : 0x80) ||
            crc(reply.bytes,5)!=(uint16_t)(reply.bytes[5] | (uint16_t)reply.bytes[6]<<8)) return 0;
        seen |= 1u<<reply.port;
    }
    return seen==3;
}
static void check(void *arg) {
    (void)arg; uint16_t values[9];
    if (!check_two_ports(TP_IDLE)) goto failed;
    if (!command(1) || !wait_for(TP_RUNNING,1,true,values)) goto failed;
    /* Keep both communication ports active while a controlled run is waiting. */
    for (unsigned i=0;i<100;i++) if (!check_two_ports(TP_RUNNING)) goto failed;
    /* Each completed step must wait for a separate advance command. */
    for(uint16_t next=1;next<=5;next++) {
        if (!wait_for(TP_RUNNING,next,true,values) || !command(5)) goto failed;
    }
    if (!wait_for(TP_COMPLETE,5,false,values) || values[1] || values[6]!=1 || get_valves()!=0) goto failed;
    uint32_t bits=(uint32_t)values[4]<<16 | values[5]; float result;
    memcpy(&result,&bits,sizeof(result));
    if (fabsf(result-0.30103f)>0.0001f || !query_last_result(result)) goto failed;
    /* A second run is stopped while awaiting the next controlled step. */
    if (!command(1) || !wait_for(TP_RUNNING,1,true,values) || get_valves()!=1 ||
        !command(2) || !status(values) || values[0]!=TP_FAILED ||
        values[1]!=TP_FAULT_STOP || values[6]!=0 || get_valves()!=0) goto failed;
    /* Save uses a separate FreeRTOS storage task; a failed second save must
     * leave the first committed record readable. */
    if (!command(3) || !set_period(123) || !command(4) || !wait_save(3)) goto failed;
    tp_config saved; uint32_t sequence;
    if (tp_store_load(&store,&saved,&sequence)!=TP_OK || saved.period_seconds!=123) goto failed;
    if (!set_period(456)) goto failed;
    atomic_store(&fail_erase,true);
    if (!command(4) || !wait_save(4)) goto failed;
    atomic_store(&fail_erase,false);
    if (tp_store_load(&store,&saved,&sequence)!=TP_OK || saved.period_seconds!=123) goto failed;
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
    const uint8_t addresses[]={1,2};
    const tp_config config={.calibration={1,0,0,0.01f,10},.output_max=1,
        .integral_max=20,.max_temperature_c=80,.period_seconds=3600,.slave_address=1};
    memset(slots,0xff,sizeof(slots));
    memset(log_bytes,0xff,sizeof(log_bytes));
    tp_event_log_io log_io={NULL,log_read,log_write,sizeof(log_bytes)};
    tp_runtime_io io={.send=send_response,.event=record_event,.poll_log=poll_log};
    tp_init(&controller,&hal,&calibration,&pid,80);
    responses=xQueueCreate(4,sizeof(response));
    if (!responses || tp_service_init(&service,&controller,recipe,sizeof(recipe)/sizeof(recipe[0]),0)!=TP_OK ||
        tp_service_attach_log(&service,NULL,request_log)!=TP_OK ||
        tp_freertos_event_log_start(&events,&log_io,4096,1)!=TP_OK ||
        tp_runtime_init(&runtime,&service,&io,addresses,2,10)!=TP_OK ||
        tp_runtime_attach_storage(&runtime,&store,&config)!=TP_OK ||
        tp_runtime_start(&runtime,4096,2)!=TP_OK ||
        xTaskCreate(check,"check",4096,NULL,1,NULL)!=pdPASS) return 1;
    vTaskStartScheduler(); return 1;
}
