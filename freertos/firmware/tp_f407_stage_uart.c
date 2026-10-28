#include "tp_f407_stage_uart.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "stage UART requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit ticks required");
#define RX_EVENTS 512u
#define TX_TIMEOUT_MS 200u
#define RX_FLAGS (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)
typedef struct { uint32_t ms; uint8_t byte; bool error; } event;
static tp_f407_stage_uart *instance;
static void start(tp_f407_stage_uart *u) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
    (void)RCC->AHB1ENR; (void)RCC->APB2ENR;
    LL_GPIO_SetAFPin_0_7(GPIOC,LL_GPIO_PIN_6,LL_GPIO_AF_8);
    LL_GPIO_SetAFPin_0_7(GPIOC,LL_GPIO_PIN_7,LL_GPIO_AF_8);
    LL_GPIO_SetPinPull(GPIOC,LL_GPIO_PIN_7,LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOC,LL_GPIO_PIN_6,LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinMode(GPIOC,LL_GPIO_PIN_6,LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOC,LL_GPIO_PIN_7,LL_GPIO_MODE_ALTERNATE);
    USART6->CR1=0; USART6->CR2=0; USART6->CR3=USART_CR3_EIE;
    USART6->BRR=139;
    (void)USART6->SR; (void)USART6->DR;
    NVIC_SetPriority(USART6_IRQn,configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(USART6_IRQn); NVIC_EnableIRQ(USART6_IRQn);
    u->started=true;
    USART6->CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE|USART_CR1_RXNEIE|USART_CR1_PEIE;
}
void USART6_IRQHandler(void) {
    uint32_t sr=USART6->SR;
    if(!(sr & RX_FLAGS)) return;
    uint8_t byte=(uint8_t)USART6->DR;
    tp_f407_stage_uart *u=instance;
    if(!u) return;
    event e={(uint32_t)xTaskGetTickCountFromISR(),byte,(sr & (RX_FLAGS & ~USART_SR_RXNE))!=0};
    BaseType_t wake=pdFALSE;
    if(xQueueSendFromISR(u->events,&e,&wake)!=pdPASS) u->overflow=true;
    portYIELD_FROM_ISR(wake);
}
static tp_result receive(void *ctx,uint8_t *byte,uint32_t *ms) {
    tp_f407_stage_uart *u=ctx;
    if(!u || !byte || !ms || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING) return TP_IO_ERROR;
    if(!u->started) start(u);
    event e; tp_result result=TP_BUSY;
    taskENTER_CRITICAL();
    if(u->overflow) { u->overflow=false; (void)xQueueReset(u->events); result=TP_IO_ERROR; }
    else if(xQueueReceive(u->events,&e,0)==pdPASS) {
        if(e.error) { (void)xQueueReset(u->events); result=TP_IO_ERROR; }
        else { *byte=e.byte; *ms=e.ms; result=TP_OK; }
    }
    taskEXIT_CRITICAL();
    return result;
}
static tp_result send(void *ctx,const uint8_t *data,size_t n) {
    tp_f407_stage_uart *u=ctx;
    if(!u || !u->started || (!data && n)) return TP_IO_ERROR;
    uint32_t since=(uint32_t)xTaskGetTickCount();
    for(size_t i=0;i<n;i++) {
        while(!(USART6->SR & USART_SR_TXE)) {
            if((uint32_t)(xTaskGetTickCount()-since)>=TX_TIMEOUT_MS) return TP_IO_ERROR;
            vTaskDelay(1);
        }
        USART6->DR=data[i];
    }
    while(!(USART6->SR & USART_SR_TC)) {
        if((uint32_t)(xTaskGetTickCount()-since)>=TX_TIMEOUT_MS) return TP_IO_ERROR;
        vTaskDelay(1);
    }
    return TP_OK;
}
static void finished(void *ctx,tp_result result) {
    tp_f407_stage_uart *u=ctx; u->finished(u->finished_ctx,result);
}
tp_result tp_f407_stage_uart_init(tp_f407_stage_uart *u,tp_stage_transport_io *io,
                                  void *ctx,void (*done)(void *,tp_result)) {
    if(!u || !io || !done || instance || SystemCoreClock!=16000000u ||
       (RCC->CFGR & (RCC_CFGR_SWS|RCC_CFGR_HPRE|RCC_CFGR_PPRE2))!=0 ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(u,0,sizeof(*u));
    u->events=xQueueCreate(RX_EVENTS,sizeof(event));
    if(!u->events) return TP_IO_ERROR;
    u->finished_ctx=ctx; u->finished=done; instance=u;
    *io=(tp_stage_transport_io){u,receive,send,finished};
    return TP_OK;
}
