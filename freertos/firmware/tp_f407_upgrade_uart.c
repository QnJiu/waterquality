#include "tp_f407_upgrade_uart.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include <string.h>

#if configTICK_RATE_HZ != 1000
#error "Upgrade UART requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t) >= 4, "32-bit ticks required");
#define RX_FLAGS (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)
#define RX_EVENTS 256u
#define TX_TIMEOUT_MS 200u
static tp_f407_upgrade_uart *instance;

/* The response is sent in task context and need not outlive this call. */
static tp_result send_response(void *ctx, const uint8_t *data, size_t length) {
    (void)ctx;
    uint32_t start = (uint32_t)xTaskGetTickCount();
    for (size_t i=0; i<length; ++i) {
        while (!(USART2->SR & USART_SR_TXE)) {
            if ((uint32_t)(xTaskGetTickCount()-start) >= TX_TIMEOUT_MS) return TP_IO_ERROR;
            vTaskDelay(1);
        }
        USART2->DR = data[i];
    }
    while (!(USART2->SR & USART_SR_TC)) {
        if ((uint32_t)(xTaskGetTickCount()-start) >= TX_TIMEOUT_MS) return TP_IO_ERROR;
        vTaskDelay(1);
    }
    return TP_OK;
}
static void hardware_start(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_7);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_3, LL_GPIO_AF_7);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_2, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_ALTERNATE);
    USART2->CR1=0; USART2->CR2=0; USART2->CR3=USART_CR3_EIE;
    USART2->BRR=139; /* 16 MHz / 115200 */
    (void)USART2->SR; (void)USART2->DR;
    NVIC_SetPriority(USART2_IRQn,configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(USART2_IRQn); NVIC_EnableIRQ(USART2_IRQn);
    USART2->CR1=USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_PEIE;
}
void USART2_IRQHandler(void) {
    uint32_t sr=USART2->SR;
    if (!(sr & RX_FLAGS)) return;
    uint8_t byte=(uint8_t)USART2->DR;
    tp_f407_upgrade_uart *u=instance;
    if (!u) return;
    BaseType_t wake=pdFALSE;
    tp_upgrade_uart_event event={(uint32_t)xTaskGetTickCountFromISR(),byte,
                                  (sr & (RX_FLAGS & ~USART_SR_RXNE)) != 0};
    if (xQueueSendFromISR(u->events,&event,&wake)!=pdPASS) u->overflow=true;
    vTaskNotifyGiveFromISR(u->task,&wake);
    portYIELD_FROM_ISR(wake);
}
static void upgrade_step(tp_f407_upgrade_uart *u) {
    taskENTER_CRITICAL();
    bool lost=u->overflow;
    if (lost) { u->overflow=false; (void)xQueueReset(u->events); }
    taskEXIT_CRITICAL();
    if (lost) tp_upgrade_stream_reset(&u->stream);
    tp_upgrade_uart_event event;
    while (xQueueReceive(u->events,&event,0)==pdPASS) {
        if (u->overflow) break;
        if (event.error) tp_upgrade_stream_reset(&u->stream);
        else {
            tp_result r=tp_upgrade_stream_byte(&u->stream,event.byte,event.ms);
            if (u->finish_pending) {
                u->finish_pending=false;
                if (r==TP_OK && u->finish_sent) { u->finish_sent(u->finish_ctx); return; }
            }
        }
    }
    taskENTER_CRITICAL();
    bool idle=!u->overflow && !uxQueueMessagesWaiting(u->events) && !(USART2->SR & RX_FLAGS);
    uint32_t now=(uint32_t)xTaskGetTickCount();
    if (idle) tp_upgrade_stream_poll(&u->stream,now);
    taskEXIT_CRITICAL();
}
static void upgrade_task(void *arg) {
    tp_f407_upgrade_uart *u=arg;
    hardware_start();
    for (;;) {
        upgrade_step(u);
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(5));
    }
}
static tp_result shared_execute(void *ctx,const tp_upgrade_commands *commands,tp_upgrade_command command,
                                const uint8_t *data,size_t length,tp_upgrade_reply *reply) {
    tp_f407_upgrade_uart *u=ctx;
    bool valid=(command==TP_UP_CMD_STATUS && length==0) ||
        (command==TP_UP_CMD_BEGIN && length>0 && data) ||
        (command==TP_UP_CMD_RESUME && length==0) ||
        (command==TP_UP_CMD_WRITE && length>4 && length<=4+TP_DOWNLOAD_CHUNK_MAX && data) ||
        (command==TP_UP_CMD_FINISH && length==0);
    if (!valid) { memset(reply,0,sizeof(*reply)); reply->result=TP_INVALID; return TP_INVALID; }
    if (command!=TP_UP_CMD_STATUS && u->request_maintenance) {
        tp_result gate=u->request_maintenance(u->maintenance_ctx);
        if (gate!=TP_OK) { memset(reply,0,sizeof(*reply)); reply->result=gate; return gate; }
    }
    if (!u->try_lock(u->lock_ctx)) {
        memset(reply,0,sizeof(*reply)); reply->result=TP_LOCK_BUSY; return TP_LOCK_BUSY;
    }
    tp_result r=tp_upgrade_command_execute(commands,command,data,length,reply);
    u->unlock(u->lock_ctx);
    if (command==TP_UP_CMD_FINISH && r==TP_OK && reply->result==TP_OK) u->finish_pending=true;
    return r;
}
static tp_result init(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
                      uint32_t timeout,uint16_t stack,UBaseType_t priority,void *lock_ctx,
                      bool (*try_lock)(void *),void (*unlock)(void *),
                      void *maintenance_ctx,tp_result (*maintenance)(void *)) {
    if (!u || !commands || !commands->upgrade || !commands->authenticate || instance || !stack ||
        (!!try_lock != !!unlock) || (maintenance && !try_lock) ||
        priority >= configMAX_PRIORITIES || SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) != 0 ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)
        return TP_INVALID;
    memset(u,0,sizeof(*u));
    tp_result r=tp_upgrade_stream_init(&u->stream,commands,u,send_response,timeout);
    if (r!=TP_OK) return r;
    if (try_lock) {
        u->maintenance_ctx=maintenance_ctx; u->request_maintenance=maintenance;
        u->lock_ctx=lock_ctx; u->try_lock=try_lock; u->unlock=unlock;
        tp_upgrade_wire_set_executor(&u->stream.wire,u,shared_execute);
    }
    u->events=xQueueCreate(RX_EVENTS,sizeof(tp_upgrade_uart_event));
    if (!u->events) return TP_IO_ERROR;
    instance=u;
    if (xTaskCreate(upgrade_task,"tp-upgrade",stack,u,priority,&u->task)!=pdPASS) {
        instance=NULL; vQueueDelete(u->events); u->events=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}

tp_result tp_f407_upgrade_uart_init(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
                                    uint32_t timeout,uint16_t stack,UBaseType_t priority) {
    return init(u,commands,timeout,stack,priority,NULL,NULL,NULL,NULL,NULL);
}
tp_result tp_f407_upgrade_uart_init_shared(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
    uint32_t timeout,uint16_t stack,UBaseType_t priority,void *ctx,
    bool (*try_lock)(void *),void (*unlock)(void *)) {
    if (!try_lock || !unlock) return TP_INVALID;
    return init(u,commands,timeout,stack,priority,ctx,try_lock,unlock,NULL,NULL);
}

tp_result tp_f407_upgrade_uart_init_guarded(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
    uint32_t timeout,uint16_t stack,UBaseType_t priority,void *ctx,
    bool (*try_lock)(void *),void (*unlock)(void *),void *maintenance_ctx,
    tp_result (*maintenance)(void *)) {
    if (!try_lock || !unlock || !maintenance) return TP_INVALID;
    return init(u,commands,timeout,stack,priority,ctx,try_lock,unlock,maintenance_ctx,maintenance);
}

tp_result tp_f407_upgrade_uart_on_finish(tp_f407_upgrade_uart *u,void *ctx,void (*finished)(void *)) {
    if (!u || u!=instance || !u->request_maintenance || !finished || u->finish_sent ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    u->finish_ctx=ctx; u->finish_sent=finished; return TP_OK;
}
