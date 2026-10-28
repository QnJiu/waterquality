#include <assert.h>
#include "../firmware/tp_f407_uart4_subordinate.c"
RCC_TypeDef fake_rcc;
USART_TypeDef fake_usart;
GPIO_TypeDef fake_gpio;
TIM_TypeDef fake_tim;
static uint32_t ticks;
static unsigned queued, errors, wake_changes, exchange_ticks;
static bool accept_buffered;
static bool wake_level;
static BaseType_t scheduler=taskSCHEDULER_NOT_STARTED;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return ticks; }
TickType_t xTaskGetTickCountFromISR(void) { return ticks; }
QueueHandle_t xQueueCreate(unsigned n,size_t size) { (void)n; (void)size; return &queued; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
BaseType_t xQueueReset(QueueHandle_t q) { (void)q; queued=0; return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q,void *out,TickType_t wait) { (void)q; (void)out; (void)wait; return 0; }
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *in,BaseType_t *wake) { (void)q; (void)in; (void)wake; return pdPASS; }
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { (void)q; return queued; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *ctx,UBaseType_t prio,TaskHandle_t *out) { (void)fn; (void)name; (void)stack; (void)ctx; (void)prio; *out=&queued; return pdPASS; }
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake) { (void)task; (void)wake; }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) { (void)clear; (void)wait; return 0; }
tp_result tp_f407_timebase_init(void) { return TP_OK; }
tp_result tp_freertos_subordinate_init(tp_freertos_subordinate *s,void *ctx,tp_sub_send send,uint32_t baud,uint8_t bits,uint32_t period,uint32_t timeout,uint32_t freshness,uint32_t first) {
    (void)ctx; (void)send; (void)baud; (void)bits; (void)period; (void)freshness; (void)first;
    s->link.poll.master.response_timeout_ms=timeout; s->snapshot=&queued; return TP_OK;
}
tp_result tp_freertos_subordinate_error(tp_freertos_subordinate *s,uint32_t us,uint32_t ms) { (void)us; (void)ms; errors++; s->link.poll.master.pending=false; s->link.poll.master.have_status=false; return TP_OK; }
tp_result tp_freertos_subordinate_byte_at(tp_freertos_subordinate *s,uint8_t b,uint32_t us,uint32_t arrived,uint32_t now) { (void)s;(void)b;(void)us;(void)arrived;(void)now;return TP_OK; }
tp_result tp_freertos_subordinate_tick(tp_freertos_subordinate *s,uint32_t us,uint32_t ms) {
    (void)us; exchange_ticks++;
    if(accept_buffered && s->link.poll.master.pending) {
        s->link.poll.master.pending=false; s->link.poll.master.have_status=true;
    } else tp_sub_master_tick(&s->link.poll.master,ms);
    return TP_OK;
}
static void wake(void *ctx,bool active) { assert(ctx==&wake_changes); wake_level=active; wake_changes++; }
int main(void) {
    tp_f407_uart4_subordinate u;
    assert(tp_f407_uart4_subordinate_init(&u,50,20,100,1,128,2)==TP_OK);
    assert(tp_f407_uart4_subordinate_set_wakeup(&u,&wake_changes,wake,19)==TP_INVALID);
    assert(tp_f407_uart4_subordinate_set_wakeup(&u,&wake_changes,wake,3)==TP_OK);
    assert(!wake_level && wake_changes==1);
    scheduler=taskSCHEDULER_RUNNING;
    uint8_t frame[TP_SUB_REQUEST_SIZE]={1};
    u.subordinate.link.poll.master.pending=true; ticks=UINT32_MAX-1;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK && wake_level && u.wake_waiting);
    service_wakeup(&u,1); assert(!u.transmitting); /* three ticks is not a full 3 ms */
    service_wakeup(&u,2); assert(u.transmitting && !u.wake_waiting && (fake_usart.CR1 & USART_CR1_TXEIE));
    u.subordinate.link.poll.master.pending=false;
    service_wakeup(&u,3); assert(wake_level); /* wait for the last stop bit */
    u.transmitting=false; service_wakeup(&u,4); assert(!wake_level);
    ticks=10; u.subordinate.link.poll.master.pending=true;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK);
    queued=1; service_wakeup(&u,14);
    assert(errors==1 && !wake_level && !u.subordinate.link.poll.master.pending && !u.transmitting);
    queued=0; ticks=20; u.subordinate.link.poll.master.pending=true;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK);
    u.subordinate.link.poll.master.pending=false; service_wakeup(&u,40);
    assert(!wake_level && !u.transmitting); /* timed out before sending */
    /* Process a buffered response before checking its deadline. */
    ticks=100; u.subordinate.link.poll.master.pending=true;
    u.subordinate.link.poll.master.deadline_start=100;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK);
    service_exchange(&u,true,0,104); assert(u.transmitting);
    u.transmitting=false; accept_buffered=true;
    service_exchange(&u,true,0,125);
    assert(exchange_ticks==1 && u.subordinate.link.poll.master.have_status && !wake_level);
    accept_buffered=false;
    /* A response in the UART queue must be handled before timeout. */
    ticks=150; u.subordinate.link.poll.master.pending=true;
    u.subordinate.link.poll.master.deadline_start=150;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK);
    service_exchange(&u,true,0,154); u.transmitting=false;
    service_exchange(&u,false,0,175);
    assert(u.subordinate.link.poll.master.pending && wake_level);
    service_exchange(&u,true,0,175);
    assert(!u.subordinate.link.poll.master.pending && !wake_level);
    /* Wake delay expires when sending cannot start in time. */
    ticks=200; u.subordinate.link.poll.master.pending=true;
    u.subordinate.link.poll.master.deadline_start=200;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK);
    service_exchange(&u,false,0,220);
    assert(!wake_level && !u.transmitting);
    /* Exercise the reference GPIO adapter separately from the custom callback. */
    scheduler=taskSCHEDULER_NOT_STARTED; u.set_wake=NULL;
    fake_rcc.AHB1ENR=0; fake_gpio.BSRR=0;
    assert(tp_f407_uart4_subordinate_use_pc8_wakeup(&u,19)==TP_INVALID);
    assert(fake_rcc.AHB1ENR==0 && fake_gpio.BSRR==0);
    assert(tp_f407_uart4_subordinate_use_pc8_wakeup(&u,3)==TP_OK);
    assert((fake_rcc.AHB1ENR & RCC_AHB1ENR_GPIOCEN) && fake_gpio.BSRR==(256u<<16));
    scheduler=taskSCHEDULER_RUNNING; ticks=50; u.subordinate.link.poll.master.pending=true;
    assert(send_request(&u,frame,sizeof(frame))==TP_OK && fake_gpio.BSRR==256u);
    u.subordinate.link.poll.master.pending=false; service_wakeup(&u,51);
    assert(fake_gpio.BSRR==(256u<<16));
    return 0;
}
