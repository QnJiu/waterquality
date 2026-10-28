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
static bool unlocked, fail_open;
static unsigned open_commands;
static unsigned door_faults;
static uint8_t reader_rx[TP_PN532_FRAME_MAX];
static size_t reader_rx_len;
static tp_result recovery_result=TP_BUSY;
static tp_result recover_door(void *ctx) {
    if(recovery_result==TP_OK) return tp_door_service_recover(ctx);
    return recovery_result;
}
static void reader_ack(void) {
    const uint8_t b[]={0,0,0xff,0,0xff,0};
    memcpy(reader_rx,b,sizeof(b)); reader_rx_len=sizeof(b);
}
static void reader_response(uint8_t command,const uint8_t *data,size_t n) {
    uint8_t *b=reader_rx; b[0]=0; b[1]=0; b[2]=0xff; b[3]=(uint8_t)(n+2); b[4]=(uint8_t)(0-b[3]);
    b[5]=0xd5; b[6]=(uint8_t)(command+1); uint8_t sum=(uint8_t)(b[5]+b[6]);
    for(size_t i=0;i<n;i++) { b[7+i]=data[i]; sum=(uint8_t)(sum+data[i]); }
    b[7+n]=(uint8_t)(0-sum); b[8+n]=0; reader_rx_len=n+9;
}
static void door_fault(void *ctx) { assert(ctx==&door_faults); door_faults++; }
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
static tp_result lock_output(void *ctx,bool open) { (void)ctx; if(open) { open_commands++; if(fail_open) return TP_IO_ERROR; } unlocked=open; return TP_OK; }
static tp_result reader_send(void *ctx,const uint8_t *data,size_t n) { (void)ctx; (void)data; (void)n; return TP_OK; }
static tp_result reader_receive(void *ctx,uint8_t *data,size_t cap,size_t *n) {
    (void)ctx; if(!reader_rx_len) return TP_BUSY;
    assert(cap>=reader_rx_len); memcpy(data,reader_rx,reader_rx_len); *n=reader_rx_len; reader_rx_len=0; return TP_OK;
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
    assert(idle_wait(&s,0)==10);
    count=1; assert(idle_wait(&s,0)==1); count=0;
    s.droplets_pending=1; assert(idle_wait(&s,0)==1); s.droplets_pending=0;
    s.droplets_overflow=true; assert(idle_wait(&s,0)==1); s.droplets_overflow=false;
    s.authorization_pending=true; assert(idle_wait(&s,0)==1); s.authorization_pending=false;
    s.recovery_inflight=true; assert(idle_wait(&s,0)==1); s.recovery_inflight=false;
    s.transmitting=true; assert(idle_wait(&s,0)==1); s.transmitting=false;
    fake_usart.ISR=USART_ISR_RXNE; assert(idle_wait(&s,0)==1); fake_usart.ISR=0;
    s.app.node.link.receiver.receiving=true; assert(idle_wait(&s,0)==1);
    s.app.node.link.receiver.receiving=false;
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
    assert(tp_f030_subordinate_on_door_fault(&s,door_fault,&door_faults)==TP_OK);
    assert(tp_f030_subordinate_set_door_recovery(&s,&door,recover_door)==TP_OK);
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
    /* A fresh authorization arriving at the reader deadline must not open the lock. */
    clock_ms=1039;
    assert(tp_door_observe(&door.door,clock_ms,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK);
    unsigned opens_before_timeout=open_commands;
    clock_ms=1040; service_once(&s);
    assert(door_faults==1 && door.door.state==TP_DOOR_FAULT && !unlocked);
    assert(open_commands==opens_before_timeout);
    service_once(&s); assert(door_faults==1);
    assert(tp_f030_subordinate_on_door_fault(&s,door_fault,&door_faults)==TP_INVALID);

    /* Output failure during authorization is reported in this same service pass. */
    assert(tp_door_service_init(&door,&reader,&output,NULL,request,1000,50,1,20,100,20)==TP_OK);
    s.door_fault_reported=false; clock_ms=1100;
    assert(tp_door_observe(&door.door,clock_ms,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK);
    fail_open=true; service_once(&s); fail_open=false;
    assert(door_faults==2 && door.door.state==TP_DOOR_FAULT && !unlocked);
    assert(s.app.door_result==TP_IO_ERROR);

    /* Recovery is serialized, keeps the fault visible, and discards queued authorization. */
    s.app.door_fault_is_device_fault=true;
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK);
    assert(tp_f030_subordinate_recover_door(&s)==TP_OK);
    assert(tp_f030_subordinate_recover_door(&s)==TP_BUSY);
    service_once(&s);
    assert(s.recovering && s.app.door_unavailable && !unlocked && door_faults==2);
    tp_sub_inputs observed;
    assert(s.app.node.io.read_inputs(s.app.node.io.ctx,&observed)==TP_OK && observed.device_fault);
    recovery_result=TP_OK; clock_ms++; service_once(&s);
    assert(s.verifying_reader && !s.recovering && door_faults==2 && s.app.door_unavailable);
    assert(tp_f030_subordinate_recover_door(&s)==TP_BUSY);
    clock_ms++; service_once(&s);
    reader_ack(); clock_ms++; service_once(&s);
    reader_response(0x14,NULL,0); clock_ms++; service_once(&s);
    reader_ack(); clock_ms++; service_once(&s);
    reader_response(0x32,NULL,0); clock_ms++; service_once(&s);
    reader_ack(); clock_ms++; service_once(&s);
    assert(s.app.door_unavailable && door_faults==2);
    const uint8_t no_card[]={0}; reader_response(0x4a,no_card,sizeof(no_card));
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_BUSY);
    clock_ms++; service_once(&s);
    assert(!s.app.door_unavailable && !s.verifying_reader && !unlocked && door_faults==2);
    assert(s.app.node.io.read_inputs(s.app.node.io.ctx,&observed)==TP_OK && !observed.device_fault);
    uint32_t old_id=id;
    assert(tp_door_observe(&door.door,clock_ms,card,sizeof(card),&new_request,&id)==TP_OK && new_request);
    assert(id!=old_id);
    assert(tp_f030_subordinate_authorize(&s,old_id,true)==TP_OK); service_once(&s);
    assert(!unlocked && door.door.auth_pending);
    assert(tp_f030_subordinate_authorize(&s,id,true)==TP_OK); service_once(&s);
    assert(unlocked);
    (void)tp_door_reader_fault(&door.door); service_once(&s);
    assert(door_faults==3 && !unlocked);
    recovery_result=TP_IO_ERROR;
    assert(tp_f030_subordinate_recover_door(&s)==TP_OK); service_once(&s);
    assert(door_faults==4 && door.door.state==TP_DOOR_FAULT && s.app.door_unavailable);
    recovery_result=TP_OK;
    assert(tp_f030_subordinate_recover_door(&s)==TP_OK); service_once(&s);
    assert(s.verifying_reader && door_faults==4);
    clock_ms++; service_once(&s); clock_ms+=1000; service_once(&s);
    assert(door_faults==5 && door.door.state==TP_DOOR_FAULT && !s.recovery_inflight);
    assert(tp_f030_subordinate_recover_door(&s)==TP_OK);

    /* At startup the optional hardware reset precedes all reader traffic. */
    assert(tp_door_service_init(&door,&reader,&output,NULL,request,1000,50,1,20,100,20)==TP_OK);
    s.recovering=false; s.verifying_reader=false; s.recovery_inflight=false;
    s.recovery_requested=false; s.app.door_unavailable=false;
    unsigned faults_before_start=door_faults;
    start_reader(&s);
    assert(s.recovering && s.recovery_inflight && s.app.door_unavailable);
    assert(door.door.state==TP_DOOR_FAULT && !unlocked && door_faults==faults_before_start+1);
    recovery_result=TP_BUSY; service_once(&s);
    assert(s.recovering && s.app.door_unavailable);
    recovery_result=TP_OK; service_once(&s);
    assert(s.verifying_reader && s.app.door_unavailable);

    timer_high=65536u; fake_tim.CNT=7; fake_tim.SR=TIM_SR_UIF;
    assert(time_us()==131079u); TIM3_IRQHandler(); fake_tim.SR=0;
    assert(time_us()==131079u);
    tp_f030_subordinate_add_sleep_us(2929u); assert(time_us()==134008u);
    timer_high=0xffff0000u; fake_tim.CNT=7; fake_tim.SR=TIM_SR_UIF;
    assert(time_us()==7u); TIM3_IRQHandler(); fake_tim.SR=0; assert(time_us()==7u);
    return 0;
}
