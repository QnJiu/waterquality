#include <assert.h>
#include <string.h>
/* Include the port to exercise one task iteration with fake registers and RTOS. */
#include "../firmware/tp_f407_upgrade_uart.c"
RCC_TypeDef fake_rcc;
USART_TypeDef fake_usart;
GPIO_TypeDef fake_gpio;
uint32_t SystemCoreClock=16000000u;
static tp_upgrade_uart_event queue[RX_EVENTS];
static unsigned head,count;
static TickType_t ticks;
static unsigned commands_run;
static bool task_failure, held, unavailable;
static unsigned unlocks;
static uint8_t response[128]; static size_t response_len;
static bool try_lock(void *ctx) { assert(ctx==&held); if(unavailable) return false; assert(!held); held=true; return true; }
static void unlock(void *ctx) { assert(ctx==&held && held); held=false; unlocks++; }
static tp_result capture(void *ctx,const uint8_t *data,size_t n) {
    (void)ctx; assert(!held && n<=sizeof(response)); memcpy(response,data,n); response_len=n; return TP_OK;
}
QueueHandle_t xQueueCreate(unsigned n,size_t size) {
    assert(n==RX_EVENTS && size==sizeof(queue[0])); head=count=0; return queue;
}
void vQueueDelete(QueueHandle_t q) { assert(q==queue); }
BaseType_t xQueueReset(QueueHandle_t q) { assert(q==queue); head=count=0; return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q,void *out,TickType_t wait) {
    assert(q==queue && wait==0);
    if (!count) return 0;
    *(tp_upgrade_uart_event *)out=queue[head]; head=(head+1)%RX_EVENTS; count--; return pdPASS;
}
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *in,BaseType_t *wake) {
    assert(q==queue); (void)wake;
    if (count==RX_EVENTS) return 0;
    queue[(head+count)%RX_EVENTS]=*(const tp_upgrade_uart_event *)in; count++; return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { assert(q==queue); return count; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_NOT_STARTED; }
TickType_t xTaskGetTickCount(void) { return ticks; }
TickType_t xTaskGetTickCountFromISR(void) { return ticks; }
void vTaskDelay(TickType_t delay) { ticks+=delay; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *ctx,
                       UBaseType_t priority,TaskHandle_t *out) {
    assert(fn && name && stack && ctx && priority<configMAX_PRIORITIES);
    if (task_failure) return 0;
    *out=ctx; return pdPASS;
}
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake) { assert(task); (void)wake; }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) { (void)clear; (void)wait; return 0; }
tp_result tp_upgrade_command_execute(const tp_upgrade_commands *c,tp_upgrade_command cmd,
                                     const uint8_t *data,size_t n,tp_upgrade_reply *reply) {
    (void)c; (void)data; assert(cmd==TP_UP_CMD_STATUS && n==0);
    assert(held); commands_run++; memset(reply,0,sizeof(*reply)); reply->result=TP_OK; return TP_OK;
}
static tp_result auth(void *ctx,const uint8_t *p,size_t n,tp_download_manifest *m) {
    (void)ctx; (void)p; (void)n; (void)m; return TP_INVALID;
}
static void push(uint8_t byte,uint32_t errors) {
    fake_usart.SR=USART_SR_RXNE | errors; fake_usart.DR=byte; USART2_IRQHandler();
    fake_usart.SR=USART_SR_TXE | USART_SR_TC;
}
static void frame_bytes(const uint8_t *frame,size_t start,size_t end) {
    for (size_t i=start;i<end;i++) push(frame[i],0);
}
int main(void) {
    tp_f407_upgrade_uart u; tp_upgrade upgrade={0};
    tp_upgrade_commands commands={&upgrade,NULL,auth};
    uint8_t frame[TP_UP_WIRE_ENCODED_MAX]; size_t n;
    assert(tp_upgrade_wire_request(TP_UP_CMD_STATUS,1,NULL,0,frame,sizeof(frame),&n)==TP_OK);
    SystemCoreClock=8000000u;
    assert(tp_f407_upgrade_uart_init(&u,&commands,50,2048,2)==TP_INVALID);
    SystemCoreClock=16000000u; task_failure=true;
    assert(tp_f407_upgrade_uart_init(&u,&commands,50,2048,2)==TP_IO_ERROR);
    task_failure=false;
    assert(tp_f407_upgrade_uart_init_shared(&u,&commands,50,2048,2,&held,try_lock,NULL)==TP_INVALID);
    assert(tp_f407_upgrade_uart_init_shared(&u,&commands,50,2048,2,&held,try_lock,unlock)==TP_OK);
    hardware_start(); fake_usart.SR=USART_SR_TXE | USART_SR_TC;
    u.stream.wire.send=capture;
    unavailable=true; frame_bytes(frame,0,n); upgrade_step(&u);
    assert(commands_run==0 && unlocks==0 && response_len>=10);
    /* This STATUS response has no escaped header/body bytes. */
    assert(response[0]==0x7e && response[2]==(TP_UP_CMD_STATUS|0x80) && response[7]==TP_BUSY && response[8]==0);
    unavailable=false; frame_bytes(frame,0,n); upgrade_step(&u);
    assert(commands_run==1 && unlocks==1 && !held && response[7]==TP_OK);
    u.stream.wire.send=send_response;
    /* An errored byte invalidates the partial request; the next frame works. */
    frame_bytes(frame,0,n/2); push(frame[n/2],USART_SR_FE);
    frame_bytes(frame,n/2+1,n); upgrade_step(&u); assert(commands_run==1);
    frame_bytes(frame,0,n); upgrade_step(&u); assert(commands_run==2);
    /* Overflow discards everything already queued, including a valid frame. */
    frame_bytes(frame,0,n);
    for (unsigned i=0;i<RX_EVENTS;i++) push(0,0);
    assert(u.overflow); upgrade_step(&u); assert(!u.overflow && !count && commands_run==2);
    frame_bytes(frame,0,n); upgrade_step(&u); assert(commands_run==3);
    /* Timed-out partial input cannot be combined with a later fragment. */
    frame_bytes(frame,0,n/2); upgrade_step(&u); ticks+=50; upgrade_step(&u);
    frame_bytes(frame,n/2,n); upgrade_step(&u); assert(commands_run==3);
    /* A stuck transmitter returns after a bounded wait; command already ran. */
    frame_bytes(frame,0,n); fake_usart.SR=0; uint32_t before=ticks;
    upgrade_step(&u); assert(commands_run==4 && ticks-before==TX_TIMEOUT_MS);
    fake_usart.SR=USART_SR_TXE | USART_SR_TC;
    frame_bytes(frame,0,n); upgrade_step(&u); assert(commands_run==5);
    return 0;
}
