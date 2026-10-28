#include "tp_f407_i2c.h"
#include "stm32f4xx.h"
#include <assert.h>
I2C_TypeDef i2c2_fake,i2c3_fake;
RCC_TypeDef rcc_fake;
CoreDebug_Type debug_fake;
DWT_Type dwt_fake;
GPIO_TypeDef gpioa_fake,gpiob_fake,gpioc_fake;
uint32_t SystemCoreClock=16000000;
int main(void) {
    tp_f407_i2c boot={0}, business={0};
    assert(tp_f407_i2c_init_on(&business,1)==TP_INVALID);
    assert(tp_f407_i2c_init(&boot)==TP_OK);
    assert(tp_f407_i2c_init_on(&business,3)==TP_OK);
    assert((RCC->APB1ENR & (RCC_APB1ENR_I2C2EN|RCC_APB1ENR_I2C3EN)) == (RCC_APB1ENR_I2C2EN|RCC_APB1ENR_I2C3EN));
    assert((gpioa_fake.modes & (1u<<8)) && (gpioc_fake.modes & (1u<<9)));
    i2c2_fake.SR1=i2c3_fake.SR1=I2C_SR1_SB|I2C_SR1_ADDR|I2C_SR1_TXE|I2C_SR1_BTF;
    tp_i2c_io a=tp_f407_i2c_io(&boot), b=tp_f407_i2c_io(&business);
    uint8_t x=0x31,y=0x62;
    assert(a.write(a.ctx,0x50,&x,1)==TP_OK && i2c2_fake.DR==x && i2c3_fake.DR==0);
    assert(b.write(b.ctx,0x50,&y,1)==TP_OK && i2c3_fake.DR==y && i2c2_fake.DR==x);
    i2c3_fake.SR1=I2C_SR1_BERR;
    assert(b.write(b.ctx,0x50,&y,1)==TP_IO_ERROR);
    assert(i2c2_fake.DR==x && (RCC->APB1ENR & RCC_APB1ENR_I2C2EN));
    assert(tp_f407_i2c_poll_shutdown(&business)==TP_OK);
    assert(!(RCC->APB1ENR & RCC_APB1ENR_I2C3EN) && (RCC->APB1ENR & RCC_APB1ENR_I2C2EN));
    assert(!gpioa_fake.modes && !gpioc_fake.modes && gpiob_fake.modes);
    assert(a.write(a.ctx,0x50,&x,1)==TP_OK);
    assert(tp_f407_i2c_poll_shutdown(&boot)==TP_OK);
    return 0;
}
