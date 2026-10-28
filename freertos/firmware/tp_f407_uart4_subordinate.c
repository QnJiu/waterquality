#include "tp_f407_uart4_subordinate.h"
#include "tp_f407_timebase.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include <string.h>

#define RX_EVENTS 128u
#define RX_FLAGS (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)
static tp_f407_uart4_subordinate *instance;

static tp_result send_request(void *ctx,const uint8_t *data,size_t length) {
    tp_f407_uart4_subordinate *u=ctx;
    if(!data || !length || length>sizeof(u->tx)) return TP_INVALID;
    taskENTER_CRITICAL();
    if(u->transmitting || u->wake_active || u->overflow || uxQueueMessagesWaiting(u->events) ||
       (UART4->SR & RX_FLAGS)) {
        taskEXIT_CRITICAL(); return TP_BUSY;
    }
    memcpy(u->tx,data,length); u->tx_length=(uint8_t)length; u->tx_position=0;
    if(u->set_wake) {
        u->set_wake(u->wake_ctx,true);
        u->wake_active=true; u->wake_waiting=true;
        u->wake_started_ms=(uint32_t)xTaskGetTickCount();
    } else {
        u->transmitting=true; UART4->CR1 |= USART_CR1_TXEIE;
    }
    taskEXIT_CRITICAL();
    return TP_OK;
}
static void hardware_start(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB1ENR |= RCC_APB1ENR_UART4EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_SetAFPin_8_15(GPIOC,LL_GPIO_PIN_10,LL_GPIO_AF_8);
    LL_GPIO_SetAFPin_8_15(GPIOC,LL_GPIO_PIN_11,LL_GPIO_AF_8);
    LL_GPIO_SetPinPull(GPIOC,LL_GPIO_PIN_11,LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOC,LL_GPIO_PIN_10,LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinMode(GPIOC,LL_GPIO_PIN_10,LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOC,LL_GPIO_PIN_11,LL_GPIO_MODE_ALTERNATE);
    UART4->CR1=0; UART4->CR2=0; UART4->CR3=USART_CR3_EIE; UART4->BRR=139;
    (void)UART4->SR; (void)UART4->DR;
    NVIC_SetPriority(UART4_IRQn,configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(UART4_IRQn); NVIC_EnableIRQ(UART4_IRQn);
    UART4->CR1=USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_PEIE;
}
void UART4_IRQHandler(void) {
    tp_f407_uart4_subordinate *u=instance;
    uint32_t sr=UART4->SR;
    BaseType_t wake=pdFALSE;
    if(!u) { (void)UART4->DR; return; }
    if(sr & RX_FLAGS) {
        uint8_t data=(uint8_t)UART4->DR;
        tp_sub_uart_event e={TIM5->CNT,(uint32_t)xTaskGetTickCountFromISR(),data,
                             (sr & (RX_FLAGS & ~USART_SR_RXNE))!=0};
        if(xQueueSendFromISR(u->events,&e,&wake)!=pdPASS) u->overflow=true;
        vTaskNotifyGiveFromISR(u->task,&wake);
    }
    if((sr & USART_SR_TXE) && (UART4->CR1 & USART_CR1_TXEIE)) {
        if(u->tx_position<u->tx_length) UART4->DR=u->tx[u->tx_position++];
        else { UART4->CR1 &= ~USART_CR1_TXEIE; UART4->CR1 |= USART_CR1_TCIE; }
    }
    if((sr & USART_SR_TC) && (UART4->CR1 & USART_CR1_TCIE)) {
        UART4->CR1 &= ~USART_CR1_TCIE; u->transmitting=false;
        vTaskNotifyGiveFromISR(u->task,&wake);
    }
    portYIELD_FROM_ISR(wake);
}
/* Owner task: keep the wake line asserted through the complete exchange. */
static void service_wakeup(tp_f407_uart4_subordinate *u, uint32_t now) {
    if(!u->wake_active) return;
    if(!u->subordinate.link.poll.master.pending) {
        u->wake_waiting=false;
        if(!u->transmitting) {
            u->set_wake(u->wake_ctx,false);
            u->wake_active=false;
        }
        return;
    }
    if(!u->wake_waiting || (uint32_t)(now-u->wake_started_ms)<=u->wake_delay_ms) return;
    taskENTER_CRITICAL();
    bool busy=u->overflow || uxQueueMessagesWaiting(u->events) || (UART4->SR & RX_FLAGS);
    if(!busy) {
        u->wake_waiting=false; u->transmitting=true;
        UART4->CR1 |= USART_CR1_TXEIE;
    }
    taskEXIT_CRITICAL();
    if(busy) {
        u->wake_waiting=false;
        (void)tp_freertos_subordinate_error(&u->subordinate,TIM5->CNT,now);
        u->set_wake(u->wake_ctx,false); u->wake_active=false;
    }
}
static void uart_task(void *ctx) {
    tp_f407_uart4_subordinate *u=ctx;
    hardware_start();
    for(;;) {
        tp_sub_uart_event e;
        taskENTER_CRITICAL();
        bool lost=u->overflow; uint32_t lost_at=TIM5->CNT;
        if(lost) { u->overflow=false; (void)xQueueReset(u->events); }
        taskEXIT_CRITICAL();
        if(lost) (void)tp_freertos_subordinate_error(&u->subordinate,lost_at,(uint32_t)xTaskGetTickCount());
        while(xQueueReceive(u->events,&e,0)==pdPASS) {
            uint32_t ms=(uint32_t)xTaskGetTickCount();
            if(u->wake_waiting || e.error || (uint32_t)(ms-e.ms)>0x7fffffffu)
                (void)tp_freertos_subordinate_error(&u->subordinate,e.us,ms);
            else (void)tp_freertos_subordinate_byte_at(&u->subordinate,e.byte,e.us,e.ms,ms);
            if(u->overflow) break;
        }
        taskENTER_CRITICAL();
        bool clear=!u->overflow && !uxQueueMessagesWaiting(u->events) && !(UART4->SR & RX_FLAGS);
        uint32_t now=TIM5->CNT;
        taskEXIT_CRITICAL();
        if(u->wake_active) {
            uint32_t now_ms=(uint32_t)xTaskGetTickCount();
            tp_sub_master_tick(&u->subordinate.link.poll.master,now_ms);
            service_wakeup(u,now_ms);
        }
        if(clear && !u->transmitting)
            (void)tp_freertos_subordinate_tick(&u->subordinate,now,(uint32_t)xTaskGetTickCount());
        service_wakeup(u,(uint32_t)xTaskGetTickCount());
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));
    }
}
tp_result tp_f407_uart4_subordinate_init(tp_f407_uart4_subordinate *u,
    uint32_t period,uint32_t timeout,uint32_t freshness,uint32_t first,
    uint16_t stack_words,UBaseType_t priority) {
    if(!u || instance || !stack_words || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)
        return TP_INVALID;
    tp_result r=tp_f407_timebase_init(); if(r!=TP_OK) return r;
    memset(u,0,sizeof(*u));
    u->events=xQueueCreate(RX_EVENTS,sizeof(tp_sub_uart_event));
    if(!u->events) return TP_IO_ERROR;
    r=tp_freertos_subordinate_init(&u->subordinate,u,send_request,115200,10,period,timeout,freshness,first);
    if(r!=TP_OK) { vQueueDelete(u->events); u->events=NULL; return r; }
    instance=u;
    if(xTaskCreate(uart_task,"tp-uart4",stack_words,u,priority,&u->task)!=pdPASS) {
        instance=NULL; vQueueDelete(u->subordinate.snapshot); vQueueDelete(u->events);
        u->subordinate.snapshot=NULL; u->events=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}

tp_result tp_f407_uart4_subordinate_set_wakeup(tp_f407_uart4_subordinate *u,
    void *ctx,void (*set_wake)(void *,bool),uint32_t delay_ms) {
    if(!u || u!=instance || !set_wake || u->set_wake || !delay_ms ||
       delay_ms>=u->subordinate.link.poll.master.response_timeout_ms-1u ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    u->wake_ctx=ctx; u->set_wake=set_wake; u->wake_delay_ms=delay_ms;
    set_wake(ctx,false);
    return TP_OK;
}
