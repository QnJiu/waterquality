#include "tp_f407_i2c.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include "task.h"
#define TIMEOUT_CYCLES (16u * 1000u * 30u)
#define ERROR_FLAGS (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT)
static bool elapsed(uint32_t start) { return (uint32_t)(DWT->CYCCNT-start) >= TIMEOUT_CYCLES; }
static tp_result wait_sr1(uint32_t flag, uint32_t start) {
    for (;;) {
        uint32_t sr=I2C2->SR1;
        if (sr & ERROR_FLAGS) return TP_IO_ERROR;
        if (sr & flag) return TP_OK;
        if (elapsed(start)) return TP_TIMEOUT;
    }
}
static tp_result idle(uint32_t start) {
    while (I2C2->SR2 & I2C_SR2_BUSY) if (elapsed(start)) return TP_TIMEOUT;
    return TP_OK;
}
static void configure_peripheral(void) {
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C2RST;
    I2C2->CR2=16; I2C2->CCR=80; I2C2->TRISE=17; I2C2->CR1=I2C_CR1_PE;
}
static tp_result address(uint8_t address7, bool read, uint32_t start) {
    I2C2->CR1 |= I2C_CR1_START;
    tp_result r=wait_sr1(I2C_SR1_SB,start); if (r!=TP_OK) return r;
    I2C2->DR=(uint8_t)((address7<<1) | (read?1u:0u));
    return wait_sr1(I2C_SR1_ADDR,start);
}
static void clear_addr(void) { (void)I2C2->SR1; (void)I2C2->SR2; }
static tp_result tx_bytes(const uint8_t *data, size_t n, uint32_t start) {
    for (size_t i=0;i<n;i++) {
        tp_result r=wait_sr1(I2C_SR1_TXE,start); if (r!=TP_OK) return r;
        I2C2->DR=data[i];
    }
    return wait_sr1(I2C_SR1_BTF,start);
}
static tp_result lock(tp_f407_i2c *bus) {
    if (!bus || !bus->mutex) return TP_INVALID;
    BaseType_t state=xTaskGetSchedulerState();
    /* Boot loading is single-threaded before the scheduler starts. */
    if (state==taskSCHEDULER_NOT_STARTED) return TP_OK;
    if (state!=taskSCHEDULER_RUNNING) return TP_BUSY;
    return xSemaphoreTake(bus->mutex,pdMS_TO_TICKS(30))==pdTRUE ? TP_OK : TP_BUSY;
}
static void release(tp_f407_i2c *bus, tp_result result) {
    if (result!=TP_OK) {
        I2C2->CR1 |= I2C_CR1_STOP;
        configure_peripheral(); /* A physically stuck bus remains an error. */
    }
    if (xTaskGetSchedulerState()==taskSCHEDULER_RUNNING) (void)xSemaphoreGive(bus->mutex);
}
static tp_result write_bus(void *ctx, uint8_t dev, const uint8_t *data, size_t n) {
    tp_f407_i2c *bus=ctx;
    if (!data || !n || dev>127) return TP_INVALID;
    tp_result r=lock(bus); if (r!=TP_OK) return r;
    uint32_t start=DWT->CYCCNT;
    r=idle(start);
    if (r==TP_OK) r=address(dev,false,start);
    if (r==TP_OK) { clear_addr(); r=tx_bytes(data,n,start); }
    if (r==TP_OK) I2C2->CR1 |= I2C_CR1_STOP;
    release(bus,r); return r;
}
static tp_result read_bus(void *ctx, uint8_t dev, const uint8_t *tx, size_t tn, uint8_t *rx, size_t n) {
    tp_f407_i2c *bus=ctx;
    if (!tx || tn!=1 || !rx || !n || dev>127 || n > 256u-tx[0]) return TP_INVALID;
    tp_result r=lock(bus); if (r!=TP_OK) return r;
    /* Individual one-byte random reads avoid the F4 two/three-byte receive
       sequencing; each byte has its own bounded transaction deadline. */
    for (size_t i=0;i<n && r==TP_OK;i++) {
        uint32_t start=DWT->CYCCNT; uint8_t offset=(uint8_t)(tx[0]+i);
        r=idle(start); if (r==TP_OK) r=address(dev,false,start);
        if (r==TP_OK) { clear_addr(); r=tx_bytes(&offset,1,start); }
        if (r==TP_OK) r=address(dev,true,start);
        if (r==TP_OK) {
            /* For one byte, ACK=0 before clearing ADDR; STOP immediately after.
               Keep these register accesses together against task/IRQ preemption. */
            uint32_t mask=__get_PRIMASK(); __disable_irq();
            I2C2->CR1 &= ~I2C_CR1_ACK; clear_addr(); I2C2->CR1 |= I2C_CR1_STOP;
            __set_PRIMASK(mask);
            r=wait_sr1(I2C_SR1_RXNE,start);
            if (r==TP_OK) rx[i]=(uint8_t)I2C2->DR;
        }
    }
    release(bus,r); return r;
}
tp_result tp_f407_i2c_init(tp_f407_i2c *bus) {
    if (!bus || SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1))!=0) return TP_INVALID;
    bus->mutex=xSemaphoreCreateMutex(); if (!bus->mutex) return TP_IO_ERROR;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_SetAFPin_8_15(GPIOB,LL_GPIO_PIN_10,LL_GPIO_AF_4);
    LL_GPIO_SetAFPin_8_15(GPIOB,LL_GPIO_PIN_11,LL_GPIO_AF_4);
    LL_GPIO_SetPinOutputType(GPIOB,LL_GPIO_PIN_10|LL_GPIO_PIN_11,LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinSpeed(GPIOB,LL_GPIO_PIN_10,LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinSpeed(GPIOB,LL_GPIO_PIN_11,LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(GPIOB,LL_GPIO_PIN_10,LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOB,LL_GPIO_PIN_11,LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOB,LL_GPIO_PIN_10,LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOB,LL_GPIO_PIN_11,LL_GPIO_MODE_ALTERNATE);
    configure_peripheral(); return TP_OK;
}
tp_i2c_io tp_f407_i2c_io(tp_f407_i2c *bus) {
    tp_i2c_io io={bus,write_bus,read_bus}; return io;
}
