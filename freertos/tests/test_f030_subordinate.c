#include <assert.h>
#include <string.h>
#include "../firmware_f030/tp_f030_subordinate.c"

RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio;
TIM_TypeDef fake_tim;
USART_TypeDef fake_usart;
uint32_t SystemCoreClock=8000000u;
static tp_f030_rx_event queue[32];
static unsigned head, count;
static uint32_t clock_ms;
static BaseType_t scheduler=taskSCHEDULER_NOT_STARTED;
static bool unlocked;
QueueHandle_t xQueueCreate(unsigned length,size_t size) {
    assert(length==32 && size==sizeof(queue[0])); head=count=0; return queue;
}
void vQueueDelete(QueueHandle_t q) { assert(q==queue); }
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *p,BaseType_t *wake) {
    assert(q==queue); (void)wake;
    if(count==32) return 0;
    queue[(head+count)%32]=*(const tp_f030_rx_event *)p; count++; return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *p,TickType_t wait) {
    assert(q==queue && wait==0);
    if(!count) return 0;
    *(tp_f030_rx_event *)p=queue[head]; head=(head+1)%32; count--; return pdPASS;
}
BaseType_t xQueueReset(QueueHandle_t q) { assert(q==queue); head=count=0; return pdPASS; }
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { assert(q==queue); return count; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *ctx,UBaseType_t p,TaskHandle_t *out) {
    assert(fn && name && stack && ctx && p); *out=ctx; return pdPASS;
}
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return clock_ms; }
void vTaskNotifyGiveFromISR(TaskHandle_t t,BaseType_t *wake) { assert(t); (void)wake; }
BaseType_t xTaskNotifyGive(TaskHandle_t t) { assert(t); return pdPASS; }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) { (void)clear; (void)wait; return 0; }
static tp_result inputs(void *ctx,tp_sub_inputs *out) { (void)ctx; *out=(tp_sub_inputs){0}; return TP_OK; }
static tp_result lock_output(void *ctx,bool open) { (void)ctx; unlocked=open; return TP_OK; }
static tp_result reader_send(void *ctx,const uint8_t *data,size_t n) { (void)ctx; (void)data; (void)n; return TP_OK; }
static tp_result reader_receive(void *ctx,uint8_t *data,size_t cap,size_t *n) {
    (void)ctx; (void)data; (void)cap; (void)n; return TP_BUSY;
}
static tp_result request(void *ctx,uint32_t id,const uint8_t *card,size_t n) {
    (void)ctx; (void)id; (void)card; (void)n; return TP_OK;
}
static void receive_byte(uint8_t b,uint32_t us) {
    fake_tim.CNT=us; fake_tim.SR=0; fake_usart.RDR=b; fake_usart.ISR=USART_ISR_RXNE;
    USART1_IRQHandler(); fake_usart.ISR=0;
}
static void receive_frame(const uint8_t *p,uint32_t us) {
    for(unsigned i=0;i<TP_SUB_REQUEST_SIZE;i++) receive_byte(p[i],us+i*100u);
}
static void finish_tx(uint8_t *out) {
    for(unsigned i=0;i<TP_SUB_RESPONSE_SIZE;i++) {
        fake_usart.ISR=USART_ISR_TXE; USART1_IRQHandler(); out[i]=(uint8_t)fake_usart.TDR;
    }
    fake_usart.ISR=USART_ISR_TXE; USART1_IRQHandler();
    fake_usart.ISR=USART_ISR_TC; USART1_IRQHandler(); fake_usart.ISR=0;
}
int main(void) {
    tp_f030_subordinate s; uint8_t req[TP_SUB_REQUEST_SIZE],reply[TP_SUB_RESPONSE_SIZE];
    tp_sub_master master;
    assert(tp_f030_subordinate_init(&s,NULL,inputs,NULL,false,384,2)==TP_OK);
    hardware_start(); fake_tim.SR=0;
    assert(tp_sub_master_init(&master,100,200,9)==TP_OK);
    assert(tp_sub_master_request(&master,0,req)==TP_OK);
    receive_frame(req,100); fake_tim.CNT=1000; service_once(&s); assert(!s.transmitting);
    fake_tim.CNT=3000; clock_ms=3; service_once(&s); assert(s.transmitting);
    finish_tx(reply); assert(!s.transmitting);
    assert(tp_sub_master_receive(&master,3,reply,sizeof(reply))==TP_OK);
    assert(master.status.faults==0);

    assert(tp_sub_master_request(&master,4,req)==TP_OK);
    receive_frame(req,4000);
    uint8_t old[TP_SUB_REQUEST_SIZE]; memcpy(old,req,sizeof(old));
    master.pending=false; assert(tp_sub_master_request(&master,8,req)==TP_OK);
    receive_frame(req,8000); fake_tim.CNT=8800; service_once(&s);
    assert(!s.transmitting);
    fake_tim.CNT=11000; clock_ms=11; service_once(&s); assert(s.transmitting);
    finish_tx(reply); assert(tp_sub_master_receive(&master,11,reply,sizeof(reply))==TP_OK);
    assert(reply[2]==req[2] && reply[2]!=old[2]);

    for(unsigned i=0;i<33;i++) receive_byte(0x33,12000+i*100);
    assert(s.overflow); fake_tim.CNT=15400; service_once(&s); assert(!s.overflow && count==0);
    fake_tim.CNT=18000; service_once(&s); assert(!s.transmitting);
    assert(tp_sub_master_request(&master,20,req)==TP_OK);
    receive_frame(req,20000); fake_tim.CNT=21000; service_once(&s);
    fake_tim.CNT=23000; clock_ms=23; service_once(&s); assert(s.transmitting);
    finish_tx(reply); assert(tp_sub_master_receive(&master,23,reply,sizeof(reply))==TP_OK);

    assert(tp_sub_master_request(&master,24,req)==TP_OK);
    for(unsigned i=0;i<TP_SUB_REQUEST_SIZE;i++) {
        if(i==3) {
            fake_tim.CNT=24000+i*100u; fake_usart.RDR=req[i];
            fake_usart.ISR=USART_ISR_RXNE | USART_ISR_FE;
            USART1_IRQHandler(); fake_usart.ISR=0;
        } else receive_byte(req[i],24000+i*100u);
    }
    fake_tim.CNT=25000; service_once(&s);
    fake_tim.CNT=28000; service_once(&s); assert(!s.transmitting);

    BaseType_t wake=pdFALSE;
    for(unsigned i=0;i<3;i++) tp_f030_subordinate_droplet_from_isr(&s,&wake);
    assert(s.droplets_pending==3 && s.app.node.droplets==0);
    service_once(&s); assert(s.app.node.droplets==3 && !s.app.node.count_fault);
    s.droplets_pending=UINT32_MAX;
    tp_f030_subordinate_droplet_from_isr(&s,&wake);
    assert(s.droplets_overflow);
    service_once(&s); assert(s.app.node.droplets==UINT32_MAX && s.app.node.count_fault);
    master.pending=false;
    assert(tp_sub_master_request(&master,30,req)==TP_OK);
    receive_frame(req,30000); fake_tim.CNT=31000; service_once(&s);
    fake_tim.CNT=33000; clock_ms=33; service_once(&s); assert(s.transmitting);
    finish_tx(reply); assert(tp_sub_master_receive(&master,33,reply,sizeof(reply))==TP_OK);
    assert(master.status.droplet_count==UINT32_MAX && master.status.faults==TP_SUB_DEVICE_FAULT);

    tp_pn532_io reader={NULL,reader_send,reader_receive};
    tp_door_io output={NULL,lock_output}; tp_door_service door;
    assert(tp_door_service_init(&door,&reader,&output,NULL,request,1000,50,1,20,100,5)==TP_OK);
    s.app.door=&door;
    assert(tp_f030_subordinate_authorize(&s,5,true)==TP_INVALID);
    scheduler=taskSCHEDULER_RUNNING;
    uint8_t card[]={1,2,3,4}; bool new_request=false; uint32_t id=0;
    clock_ms=40;
    assert(tp_door_observe(&door.door,clock_ms,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    assert(tp_f030_subordinate_authorize(&s,id+1,true)==TP_OK);
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_BUSY);
    service_once(&s); assert(!unlocked && door.door.auth_pending);
    assert(tp_f030_subordinate_authorize(&s,id,false)==TP_OK);
    service_once(&s); assert(!unlocked && !door.door.auth_pending);
    assert(tp_door_observe(&door.door,41,NULL,0,&new_request,&id)==TP_OK);
    assert(tp_door_observe(&door.door,42,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    clock_ms=43;
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK);
    service_once(&s); assert(unlocked && door.door.state==TP_DOOR_UNLOCKED);
    clock_ms=63; service_once(&s); assert(!unlocked);
    assert(tp_door_observe(&door.door,64,NULL,0,&new_request,&id)==TP_OK);
    assert(tp_door_observe(&door.door,65,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK);
    clock_ms=165; service_once(&s); assert(!unlocked && !door.door.auth_pending);

    timer_high=65536u; fake_tim.CNT=7; fake_tim.SR=TIM_SR_UIF;
    assert(time_us()==131079u); TIM3_IRQHandler(); fake_tim.SR=0;
    assert(time_us()==131079u);
    timer_high=0xffff0000u; fake_tim.CNT=7; fake_tim.SR=TIM_SR_UIF;
    assert(time_us()==7u); TIM3_IRQHandler(); fake_tim.SR=0; assert(time_us()==7u);
    return 0;
}
