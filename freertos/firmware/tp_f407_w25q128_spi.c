#include "tp_f407_w25q128_spi.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include "task.h"
#define DEADLINE_CYCLES 160000u
#define CS_DELAY_CYCLES 16u
#define CS_PIN LL_GPIO_PIN_4
static bool expired(uint32_t start) { return (uint32_t)(DWT->CYCCNT-start) >= DEADLINE_CYCLES; }
static void delay_cs(void) {
    uint32_t start=DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT-start)<CS_DELAY_CYCLES) { }
}
static void configure_spi(void) {
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    /* APB2 16 MHz / 16 = 1 MHz; mode 0, MSB first, 8-bit, software NSS. */
    SPI1->CR1=SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
              SPI_CR1_BR_1 | SPI_CR1_BR_0;
    SPI1->CR1 |= SPI_CR1_SPE;
}
static tp_result wait_flag(uint32_t flag, bool set, uint32_t start) {
    for (;;) {
        if (expired(start)) return TP_TIMEOUT;
        uint32_t sr=SPI1->SR;
        if (sr & (SPI_SR_OVR | SPI_SR_MODF)) return TP_IO_ERROR;
        if (((sr & flag)!=0u)==set) return TP_OK;
    }
}
static tp_result transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t n) {
    tp_f407_w25q128_spi *bus=ctx;
    if (!bus || !bus->mutex || !tx || !rx || !n || n>260u) return TP_INVALID;
    BaseType_t state=xTaskGetSchedulerState();
    if (state!=taskSCHEDULER_NOT_STARTED && state!=taskSCHEDULER_RUNNING) return TP_BUSY;
    if (state==taskSCHEDULER_RUNNING && xSemaphoreTake(bus->mutex,0)!=pdTRUE) return TP_BUSY;
    uint32_t start=DWT->CYCCNT;
    LL_GPIO_ResetOutputPin(GPIOA,CS_PIN);
    delay_cs();
    tp_result r=TP_OK;
    for (size_t i=0;i<n && r==TP_OK;i++) {
        r=wait_flag(SPI_SR_TXE,true,start);
        if (r==TP_OK) {
            *(__IO uint8_t *)&SPI1->DR=tx[i];
            r=wait_flag(SPI_SR_RXNE,true,start);
            if (r==TP_OK) rx[i]=*(__IO uint8_t *)&SPI1->DR;
        }
    }
    if (r==TP_OK) r=wait_flag(SPI_SR_BSY,false,start);
    if (r==TP_OK) delay_cs();
    LL_GPIO_SetOutputPin(GPIOA,CS_PIN);
    if (r!=TP_OK) configure_spi();
    if (state==taskSCHEDULER_RUNNING) (void)xSemaphoreGive(bus->mutex);
    return r;
}
tp_result tp_f407_w25q128_spi_init(tp_f407_w25q128_spi *bus) {
    if (!bus || SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2))!=0u) return TP_INVALID;
    bus->mutex=xSemaphoreCreateMutex();
    if (!bus->mutex) return TP_IO_ERROR;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->AHB1ENR; (void)RCC->APB2ENR;
    LL_GPIO_SetOutputPin(GPIOA,CS_PIN);
    LL_GPIO_SetPinOutputType(GPIOA,CS_PIN,LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(GPIOA,CS_PIN,LL_GPIO_MODE_OUTPUT);
    for (uint32_t pin=LL_GPIO_PIN_5; pin<=LL_GPIO_PIN_7; pin<<=1) {
        LL_GPIO_SetAFPin_0_7(GPIOA,pin,LL_GPIO_AF_5);
        LL_GPIO_SetPinSpeed(GPIOA,pin,LL_GPIO_SPEED_FREQ_HIGH);
        LL_GPIO_SetPinPull(GPIOA,pin,LL_GPIO_PULL_NO);
        LL_GPIO_SetPinMode(GPIOA,pin,LL_GPIO_MODE_ALTERNATE);
    }
    configure_spi();
    return TP_OK;
}
tp_w25q128_io tp_f407_w25q128_spi_io(tp_f407_w25q128_spi *bus) {
    tp_w25q128_io io={ .ctx=bus, .transfer=transfer };
    return io;
}
