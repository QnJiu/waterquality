#include "tp_f407_usart3_rtu.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include <string.h>
#define RX_EVENTS 256u
#define DE_PIN LL_GPIO_PIN_10
static tp_f407_usart3_rtu *instance;
static void hardware_start(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN | RCC_APB1ENR_TIM5EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_ResetOutputPin(GPIOD,DE_PIN);
    LL_GPIO_SetPinOutputType(GPIOD,DE_PIN,LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(GPIOD,DE_PIN,LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetAFPin_8_15(GPIOD,LL_GPIO_PIN_8,LL_GPIO_AF_7);
    LL_GPIO_SetAFPin_8_15(GPIOD,LL_GPIO_PIN_9,LL_GPIO_AF_7);
    LL_GPIO_SetPinPull(GPIOD,LL_GPIO_PIN_9,LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOD,LL_GPIO_PIN_8,LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinMode(GPIOD,LL_GPIO_PIN_8,LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOD,LL_GPIO_PIN_9,LL_GPIO_MODE_ALTERNATE);
    TIM5->CR1=0; TIM5->PSC=15; TIM5->ARR=0xffffffffu;
    TIM5->EGR=TIM_EGR_UG; TIM5->CNT=0; TIM5->CR1=TIM_CR1_CEN;
    USART3->CR1=0; USART3->CR2=0; USART3->CR3=USART_CR3_EIE;
    /* OVER8=0, BRR=round(16 MHz / 115200): actual baud about 115108. */
    USART3->BRR=139;
    (void)USART3->SR; (void)USART3->DR;
    NVIC_SetPriority(USART3_IRQn,configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(USART3_IRQn); NVIC_EnableIRQ(USART3_IRQn);
    USART3->CR1=USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_PEIE;
}
void USART3_IRQHandler(void) {
    tp_f407_usart3_rtu *u=instance;
    uint32_t sr=USART3->SR;
    BaseType_t wake=pdFALSE;
    if (!u) { (void)USART3->DR; return; }
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t data=(uint8_t)USART3->DR;
        tp_uart_event e={TIM5->CNT,data,(sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))!=0};
        if (xQueueSendFromISR(u->events,&e,&wake)!=pdPASS) u->overflow=true;
        vTaskNotifyGiveFromISR(u->task,&wake);
    }
    if ((sr & USART_SR_TXE) && (USART3->CR1 & USART_CR1_TXEIE)) {
        if (u->tx_pos<u->current.length) USART3->DR=u->current.data[u->tx_pos++];
        else { USART3->CR1 &= ~USART_CR1_TXEIE; USART3->CR1 |= USART_CR1_TCIE; }
    }
    if ((sr & USART_SR_TC) && (USART3->CR1 & USART_CR1_TCIE)) {
        USART3->CR1 &= ~USART_CR1_TCIE;
        LL_GPIO_ResetOutputPin(GPIOD,DE_PIN);
        USART3->CR1 |= USART_CR1_RE;
        u->transmitting=false;
        vTaskNotifyGiveFromISR(u->task,&wake);
    }
    portYIELD_FROM_ISR(wake);
}
static void uart_task(void *arg) {
    tp_f407_usart3_rtu *u=arg;
    hardware_start();
    for (;;) {
        tp_uart_event e;
        /* A lost event invalidates the whole in-flight frame, including events
         * still in the queue. Do not allow a truncated command to be submitted. */
        taskENTER_CRITICAL();
        bool lost=u->overflow;
        uint32_t lost_at=TIM5->CNT;
        if (lost) { u->overflow=false; (void)xQueueReset(u->events); }
        taskEXIT_CRITICAL();
        if (lost) tp_rtu_port_error(&u->port,lost_at);
        while (xQueueReceive(u->events,&e,0)==pdPASS) {
            if (e.error) tp_rtu_port_error(&u->port,e.at_us);
            else tp_rtu_port_byte(&u->port,e.byte,e.at_us);
            if (u->overflow) break;
        }
        /* Snapshot time while no queued or hardware byte is awaiting delivery.
         * A later byte has a later timestamp; polling can happen outside the
         * critical section without hiding a byte already in the UART. */
        taskENTER_CRITICAL();
        bool clear=!u->overflow && !uxQueueMessagesWaiting(u->events) &&
                   !(USART3->SR & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE));
        uint32_t now=TIM5->CNT;
        taskEXIT_CRITICAL();
        if (clear && !u->transmitting) tp_rtu_port_poll(&u->port,now);
        if (clear && !u->transmitting && !u->port.receiver.receiving &&
            xQueueReceive(u->responses,&u->current,0)==pdPASS) {
            bool dropped=false;
            taskENTER_CRITICAL();
            /* Recheck after dequeuing: if input arrived, discard response
             * rather than transmit over a new request. */
            if (!u->overflow && !uxQueueMessagesWaiting(u->events) &&
                !(USART3->SR & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))) {
                u->transmitting=true; u->tx_pos=0;
                USART3->CR1 &= ~USART_CR1_RE;
                LL_GPIO_SetOutputPin(GPIOD,DE_PIN);
                USART3->CR1 |= USART_CR1_TXEIE;
            } else dropped=true;
            taskEXIT_CRITICAL();
            if (dropped && u->runtime->io.communication_error)
                u->runtime->io.communication_error(u->runtime->io.ctx,u->port_number);
        }
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));
    }
}
tp_result tp_f407_usart3_rtu_init(tp_f407_usart3_rtu *u,tp_freertos_runtime *rt,
    unsigned port,uint16_t stack_words,UBaseType_t priority) {
    if (!u || !rt || instance || !stack_words || port>=rt->port_count ||
        SystemCoreClock!=16000000u || (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(u,0,sizeof(*u)); u->runtime=rt; u->port_number=port;
    tp_result r=tp_runtime_rtu_port_init(rt,&u->port,port,115200,10);
    if (r!=TP_OK) return r;
    u->events=xQueueCreate(RX_EVENTS,sizeof(tp_uart_event));
    if (!u->events) return TP_IO_ERROR;
    u->responses=xQueueCreate(2,sizeof(tp_uart_tx));
    if (!u->responses) { vQueueDelete(u->events); u->events=NULL; return TP_IO_ERROR; }
    instance=u;
    if (xTaskCreate(uart_task,"tp-uart3",stack_words,u,priority,&u->task)!=pdPASS) {
        instance=NULL; vQueueDelete(u->responses); vQueueDelete(u->events);
        u->responses=NULL; u->events=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_f407_usart3_rtu_send(tp_f407_usart3_rtu *u,unsigned port,
                                   const uint8_t *data,size_t length) {
    if (!u || !u->responses || port!=u->port_number || !data || !length || length>TP_RUNTIME_FRAME_MAX) return TP_INVALID;
    tp_uart_tx frame; frame.length=(uint16_t)length; memcpy(frame.data,data,length);
    if (xQueueSend(u->responses,&frame,0)!=pdPASS) return TP_BUSY;
    xTaskNotifyGive(u->task); return TP_OK;
}
