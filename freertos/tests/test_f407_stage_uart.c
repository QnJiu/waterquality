#include <assert.h>
#include "../firmware/tp_f407_stage_uart.c"
RCC_TypeDef fake_rcc;
USART_TypeDef fake_usart;
GPIO_TypeDef fake_gpio;
uint32_t SystemCoreClock=16000000u;
static event events[RX_EVENTS];
static unsigned head,count,completed;
static TickType_t ticks;
static BaseType_t scheduler=taskSCHEDULER_NOT_STARTED;
QueueHandle_t xQueueCreate(unsigned n,size_t size) { assert(n==RX_EVENTS && size==sizeof(event)); return events; }
BaseType_t xQueueReset(QueueHandle_t q) { assert(q==events); head=count=0; return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q,void *out,TickType_t wait) {
    assert(q==events && !wait); if(!count) return 0;
    *(event *)out=events[head]; head=(head+1)%RX_EVENTS; count--; return pdPASS;
}
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *in,BaseType_t *wake) {
    assert(q==events); (void)wake; if(count==RX_EVENTS) return 0;
    events[(head+count)%RX_EVENTS]=*(const event *)in; count++; return pdPASS;
}
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return ticks; }
TickType_t xTaskGetTickCountFromISR(void) { return ticks; }
void vTaskDelay(TickType_t n) { ticks+=n; }
static void done(void *ctx,tp_result r) { assert(ctx==&completed && r==TP_OK); completed++; }
static void push(uint8_t b,uint32_t flags) { fake_usart.SR=USART_SR_RXNE|flags; fake_usart.DR=b; USART6_IRQHandler(); }
int main(void) {
    tp_f407_stage_uart u; tp_stage_transport_io io; uint8_t b; uint32_t ms;
    SystemCoreClock=8000000u; assert(tp_f407_stage_uart_init(&u,&io,&completed,done)==TP_INVALID);
    SystemCoreClock=16000000u; assert(tp_f407_stage_uart_init(&u,&io,&completed,done)==TP_OK);
    assert(tp_f407_stage_uart_init(&u,&io,&completed,done)==TP_INVALID);
    scheduler=taskSCHEDULER_RUNNING;
    assert(io.receive(io.ctx,&b,&ms)==TP_BUSY && u.started);
    ticks=123; push(42,0); ticks=456;
    assert(io.receive(io.ctx,&b,&ms)==TP_OK && b==42 && ms==123);
    push(1,USART_SR_FE); push(2,0);
    assert(io.receive(io.ctx,&b,&ms)==TP_IO_ERROR && count==0);
    for(unsigned i=0;i<RX_EVENTS+1;i++) push(0,0);
    assert(io.receive(io.ctx,&b,&ms)==TP_IO_ERROR && count==0);
    push(9,0); assert(io.receive(io.ctx,&b,&ms)==TP_OK && b==9);
    fake_usart.SR=USART_SR_TXE|USART_SR_TC;
    uint8_t bytes[]={1,2,3}; assert(io.send(io.ctx,bytes,sizeof(bytes))==TP_OK && fake_usart.DR==3);
    fake_usart.SR=0; TickType_t before=ticks;
    assert(io.send(io.ctx,bytes,sizeof(bytes))==TP_IO_ERROR && ticks-before==TX_TIMEOUT_MS);
    io.finished(io.ctx,TP_OK); assert(completed==1);
    return 0;
}
