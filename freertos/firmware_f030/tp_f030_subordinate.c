#include "tp_f030_subordinate.h"
#include "stm32f0xx.h"
#include <string.h>
#ifdef TP_F030_WAKE_INPUT
#include "tp_f030_wake_input.h"
#endif
#ifdef TP_F030_REFERENCE_INPUTS
#include "tp_f030_reference_inputs.h"
#endif

#define RX_EVENTS 32u
#define RX_ERRORS (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)
#define RX_FLAGS (USART_ISR_RXNE | RX_ERRORS)
static tp_f030_subordinate *instance;
static volatile uint32_t timer_high;

/* Call with interrupts masked, or from an ISR that TIM3 cannot preempt. */
static uint32_t time_us(void) {
    uint32_t high=timer_high;
    uint32_t low=TIM3->CNT;
    if(TIM3->SR & TIM_SR_UIF) { high+=65536u; low=TIM3->CNT; }
    return high+low;
}
void TIM3_IRQHandler(void) {
    if(TIM3->SR & TIM_SR_UIF) { TIM3->SR=~TIM_SR_UIF; timer_high+=65536u; }
}
static tp_result read_inputs(void *ctx,tp_sub_inputs *out) {
    tp_f030_subordinate *s=ctx;
    return s->read_inputs(s->input_ctx,out);
}
static tp_result send_reply(void *ctx,const uint8_t *data,size_t len) {
    tp_f030_subordinate *s=ctx;
    if(!data || !len || len>sizeof(s->tx)) return TP_INVALID;
    taskENTER_CRITICAL();
    if(s->processing_rx || s->transmitting || s->overflow || uxQueueMessagesWaiting(s->rx) ||
       (USART1->ISR & RX_FLAGS)) {
        taskEXIT_CRITICAL(); return TP_BUSY;
    }
    memcpy(s->tx,data,len); s->tx_length=(uint8_t)len; s->tx_position=0;
    s->transmitting=true; USART1->CR1 |= USART_CR1_TXEIE;
    taskEXIT_CRITICAL();
    return TP_OK;
}
void USART1_IRQHandler(void) {
    tp_f030_subordinate *s=instance;
    uint32_t flags=USART1->ISR;
    BaseType_t wake=pdFALSE;
    if(!s) return;
    if(flags & RX_FLAGS) {
        tp_f030_rx_event event={time_us(),0,(flags & RX_ERRORS)!=0};
        if(flags & USART_ISR_RXNE) event.byte=(uint8_t)USART1->RDR;
        USART1->ICR=USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF;
        if(s->overflow || xQueueSendFromISR(s->rx,&event,&wake)!=pdPASS) s->overflow=true;
        vTaskNotifyGiveFromISR(s->task,&wake);
    }
    if((flags & USART_ISR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if(s->tx_position<s->tx_length) USART1->TDR=s->tx[s->tx_position++];
        else { USART1->CR1 &= ~USART_CR1_TXEIE; USART1->CR1 |= USART_CR1_TCIE; }
    }
    if((flags & USART_ISR_TC) && (USART1->CR1 & USART_CR1_TCIE)) {
        USART1->ICR=USART_ICR_TCCF; USART1->CR1 &= ~USART_CR1_TCIE;
        s->transmitting=false; vTaskNotifyGiveFromISR(s->task,&wake);
    }
    portYIELD_FROM_ISR(wake);
}
void tp_f030_subordinate_droplet_from_isr(tp_f030_subordinate *s,BaseType_t *wake) {
    if(!s || s!=instance || !wake) return;
    UBaseType_t mask=portSET_INTERRUPT_MASK_FROM_ISR();
    if(s->droplets_pending==UINT32_MAX) s->droplets_overflow=true;
    else s->droplets_pending++;
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    vTaskNotifyGiveFromISR(s->task,wake);
}
tp_result tp_f030_subordinate_authorize(tp_f030_subordinate *s,uint32_t id,bool approved) {
    if(!s || s!=instance || !s->app.door ||
       xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING) return TP_INVALID;
    taskENTER_CRITICAL();
    if(s->authorization_pending || s->recovery_inflight) { taskEXIT_CRITICAL(); return TP_BUSY; }
    s->authorization_id=id; s->authorization_approved=approved;
    s->authorization_pending=true;
    taskEXIT_CRITICAL();
    xTaskNotifyGive(s->task);
    return TP_OK;
}
tp_result tp_f030_subordinate_recover_door(tp_f030_subordinate *s) {
    if(!s || s!=instance || !s->door_recovery || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING)
        return TP_INVALID;
    taskENTER_CRITICAL();
    if(s->recovery_inflight) {
        taskEXIT_CRITICAL(); return TP_BUSY;
    }
    s->recovery_requested=true; s->recovery_inflight=true;
    taskEXIT_CRITICAL();
    xTaskNotifyGive(s->task); return TP_OK;
}
static void hardware_start(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->AHBENR; (void)RCC->APB1ENR; (void)RCC->APB2ENR;
    GPIOA->AFR[1]=(GPIOA->AFR[1] & ~((0xfu<<4)|(0xfu<<8))) | (1u<<4) | (1u<<8);
    GPIOA->PUPDR=(GPIOA->PUPDR & ~(3u<<20)) | (1u<<20);
    GPIOA->MODER=(GPIOA->MODER & ~((3u<<18)|(3u<<20))) | (2u<<18) | (2u<<20);
    TIM3->CR1=0; TIM3->PSC=7; TIM3->ARR=65535; TIM3->EGR=TIM_EGR_UG;
    TIM3->SR=0; TIM3->DIER=TIM_DIER_UIE; timer_high=0;
    NVIC_SetPriority(TIM3_IRQn,2); NVIC_ClearPendingIRQ(TIM3_IRQn); NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1=TIM_CR1_CEN;
    USART1->CR1=0; USART1->CR2=0; USART1->CR3=USART_CR3_EIE; USART1->BRR=69;
    USART1->ICR=USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF | USART_ICR_TCCF;
    NVIC_SetPriority(USART1_IRQn,2); NVIC_ClearPendingIRQ(USART1_IRQn); NVIC_EnableIRQ(USART1_IRQn);
    USART1->CR1=USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_PEIE;
}
static void service_once(tp_f030_subordinate *s) {
    tp_f030_rx_event event;
    taskENTER_CRITICAL();
    uint32_t droplets=s->droplets_pending;
    bool droplets_lost=s->droplets_overflow;
    bool authorized=s->authorization_pending;
    uint32_t authorization_id=s->authorization_id;
    bool approved=s->authorization_approved;
    s->authorization_pending=false;
    bool recovery_requested=s->recovery_requested;
    s->recovery_requested=false;
    if(recovery_requested) {
        if(s->app.door && s->app.door->door.state==TP_DOOR_FAULT) s->recovering=true;
        else s->recovery_inflight=false;
    }
    s->droplets_pending=0; s->droplets_overflow=false;
    taskEXIT_CRITICAL();
    tp_sub_node_droplets(&s->app.node,droplets);
    if(droplets_lost) tp_sub_node_count_error(&s->app.node);
    if(s->app.door) {
        uint32_t door_now=(uint32_t)xTaskGetTickCount();
        bool discard_authorization=s->recovering || s->verifying_reader;
        bool recovery_finished=false;
        if(s->recovering) {
            s->app.door_unavailable=true;
            tp_result r=s->door_recovery(s->door_recovery_ctx);
            if(r==TP_OK && s->app.door->door.state==TP_DOOR_LOCKED &&
               s->app.door->scan.state==TP_SCAN_START) {
                s->recovering=false; s->verifying_reader=true; s->door_fault_reported=false;
            } else if(r!=TP_BUSY) {
                (void)tp_door_reader_fault(&s->app.door->door);
                s->recovering=false; s->door_fault_reported=false; recovery_finished=true;
            }
            s->app.door_result=s->recovering || s->verifying_reader ? TP_BUSY : TP_IO_ERROR;
        } else {
            s->app.door_result=tp_door_service_tick(s->app.door,door_now);
            if(s->verifying_reader) {
                if(s->app.door_result==TP_OK) {
                    s->verifying_reader=false; s->app.door_unavailable=false; recovery_finished=true;
                } else if(s->app.door_result!=TP_BUSY) {
                    s->verifying_reader=false; recovery_finished=true;
                }
            }
        }
        if(authorized && !discard_authorization && !s->recovering && !s->verifying_reader && !s->app.door_unavailable &&
           (s->app.door_result==TP_OK || s->app.door_result==TP_BUSY)) {
            (void)tp_door_service_authorize(s->app.door,door_now,authorization_id,approved);
            if(s->app.door->door.state==TP_DOOR_FAULT) s->app.door_result=TP_IO_ERROR;
        }
        if(recovery_finished) {
            taskENTER_CRITICAL(); s->recovery_inflight=false; taskEXIT_CRITICAL();
        }
        if(s->app.door_result!=TP_OK && s->app.door_result!=TP_BUSY && !s->door_fault_reported) {
            s->door_fault_reported=true;
            if(s->door_fault_handler) s->door_fault_handler(s->door_fault_ctx);
        }
    }
    taskENTER_CRITICAL();
    bool lost=s->overflow;
    uint32_t now=time_us();
    if(lost) { (void)xQueueReset(s->rx); s->overflow=false; }
    taskEXIT_CRITICAL();
    if(lost) (void)tp_sub_node_error(&s->app.node,now,(uint32_t)xTaskGetTickCount());
    for(unsigned processed=0; processed<RX_EVENTS && xQueueReceive(s->rx,&event,0)==pdPASS; processed++) {
        uint32_t ms=(uint32_t)xTaskGetTickCount();
        s->processing_rx=true;
        if(event.error) (void)tp_sub_node_error(&s->app.node,event.us,ms);
        else (void)tp_sub_node_byte(&s->app.node,event.byte,event.us,ms);
        s->processing_rx=false;
        if(s->overflow) break;
    }
    taskENTER_CRITICAL();
    now=time_us();
    bool clear=!s->overflow && !uxQueueMessagesWaiting(s->rx) && !(USART1->ISR & RX_FLAGS);
    taskEXIT_CRITICAL();
    if(clear && !s->transmitting)
        (void)tp_sub_node_tick(&s->app.node,now,(uint32_t)xTaskGetTickCount());
}
/* Avoid delaying a partial frame, pending ISR event, or active door I/O.
 * Notifications arriving after this check remain pending in FreeRTOS. */
static TickType_t idle_wait(tp_f030_subordinate *s,uint32_t now_ms) {
    taskENTER_CRITICAL();
    bool idle=!s->transmitting && !s->processing_rx && !s->overflow &&
              !s->droplets_pending && !s->droplets_overflow &&
              !s->authorization_pending && !s->recovery_requested &&
              !s->recovery_inflight && !uxQueueMessagesWaiting(s->rx) &&
              !(USART1->ISR & RX_FLAGS);
    taskEXIT_CRITICAL();
    uint32_t budget=0;
    if(s->recovering || s->verifying_reader ||
       !tp_sub_app_sleep_budget(&s->app,now_ms,10u,idle,&budget)) return 1;
    return (TickType_t)budget;
}
/* Reset an optionally configured reader before the first scan.  The same
 * recovery path used after a fault keeps the device fault visible until a
 * complete reader transaction succeeds. */
static void start_reader(tp_f030_subordinate *s) {
    if (!s->app.door || !s->door_recovery) return;
    s->app.door_unavailable=true;
    (void)tp_door_reader_fault(&s->app.door->door);
    if (s->door_fault_handler) s->door_fault_handler(s->door_fault_ctx);
    s->recovering=true;
    s->recovery_inflight=true;
}
static void run(void *ctx) {
    tp_f030_subordinate *s=ctx;
    hardware_start();
    start_reader(s);
#ifdef TP_F030_WAKE_INPUT
    tp_f030_wake_input_start(s);
#endif
#ifdef TP_F030_REFERENCE_INPUTS
    tp_f030_reference_inputs_start(s);
#endif
    for(;;) {
#ifdef TP_F030_REFERENCE_INPUTS
        tp_f030_reference_inputs_button_tick((uint32_t)xTaskGetTickCount());
#endif
        service_once(s);
        (void)ulTaskNotifyTake(pdTRUE,idle_wait(s,(uint32_t)xTaskGetTickCount()));
    }
}
tp_result tp_f030_subordinate_init(tp_f030_subordinate *s,void *input_ctx,
    tp_result (*inputs)(void *,tp_sub_inputs *),tp_door_service *door,bool door_fault,
    uint16_t stack_words,UBaseType_t priority) {
    if(!s || !inputs || instance || !stack_words || priority>=configMAX_PRIORITIES ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED || SystemCoreClock!=8000000u ||
       (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE)) ||
       (RCC->CFGR3 & RCC_CFGR3_USART1SW)) return TP_INVALID;
    memset(s,0,sizeof(*s)); s->input_ctx=input_ctx; s->read_inputs=inputs;
    tp_sub_node_io io={s,send_reply,read_inputs};
    tp_result r=tp_sub_app_init(&s->app,&io,115200,10,door,door_fault);
    if(r!=TP_OK) return r;
    s->rx=xQueueCreate(RX_EVENTS,sizeof(tp_f030_rx_event));
    if(!s->rx) return TP_IO_ERROR;
    instance=s;
    if(xTaskCreate(run,"tp-sub",stack_words,s,priority,&s->task)!=pdPASS) {
        instance=NULL; vQueueDelete(s->rx); s->rx=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_f030_subordinate_on_door_fault(tp_f030_subordinate *s,void (*handler)(void *),void *ctx) {
    if(!s || s!=instance || !s->app.door || !handler ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    s->door_fault_handler=handler; s->door_fault_ctx=ctx;
    return TP_OK;
}

tp_result tp_f030_subordinate_set_door_recovery(tp_f030_subordinate *s,void *ctx,tp_result (*recover)(void *)) {
    if(!s || s!=instance || !s->app.door || !recover || s->door_recovery ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    s->door_recovery=recover; s->door_recovery_ctx=ctx; return TP_OK;
}
