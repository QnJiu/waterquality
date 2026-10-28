#include <assert.h>
#include <string.h>
#include "stm32f0xx.h"
#include "tp_f030_i2c.h"
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio_b;
I2C_TypeDef fake_i2c;
uint32_t SystemCoreClock=8000000u;

int main(void) {
    tp_f030_i2c i; uint8_t data[TP_PN532_FRAME_MAX],out[TP_PN532_FRAME_MAX];
    for(size_t n=0;n<sizeof(data);n++) data[n]=(uint8_t)n;
    assert(tp_f030_i2c_init(&i)==TP_OK);
    tp_pn532_i2c_bus bus=tp_f030_i2c_bus(&i);
    assert((fake_gpio_b.OTYPER & (3u<<6))==(3u<<6));
    assert(fake_i2c.TIMINGR==0x10420f13u && fake_i2c.CR1==I2C_CR1_PE);
    assert(bus.write_start(bus.ctx,0x24,data,sizeof(data))==TP_OK);
    assert(((fake_i2c.CR2>>16)&255u)==255u && (fake_i2c.CR2 & I2C_CR2_RELOAD));
    for(size_t n=0;n<255;n++) {
        fake_i2c.ISR=I2C_ISR_TXIS; assert(bus.write_poll(bus.ctx)==TP_BUSY);
        assert(fake_i2c.TXDR==data[n]);
    }
    fake_i2c.ISR=I2C_ISR_TCR; assert(bus.write_poll(bus.ctx)==TP_BUSY);
    assert(((fake_i2c.CR2>>16)&255u)==7u && (fake_i2c.CR2 & I2C_CR2_AUTOEND));
    for(size_t n=255;n<sizeof(data);n++) {
        fake_i2c.ISR=I2C_ISR_TXIS; assert(bus.write_poll(bus.ctx)==TP_BUSY);
        assert(fake_i2c.TXDR==data[n]);
    }
    fake_i2c.ISR=I2C_ISR_STOPF; assert(bus.write_poll(bus.ctx)==TP_OK);
    fake_i2c.ISR=0; assert(bus.read_start(bus.ctx,0x24,1,false)==TP_OK);
    fake_i2c.RXDR=0; fake_i2c.ISR=I2C_ISR_RXNE;
    assert(bus.read_poll(bus.ctx,out,1)==TP_BUSY);
    fake_i2c.ISR=I2C_ISR_TCR; assert(bus.read_poll(bus.ctx,out,1)==TP_OK && out[0]==0);
    bus.read_stop(bus.ctx); assert(fake_i2c.CR2 & I2C_CR2_STOP);
    fake_i2c.ISR=0; assert(bus.read_start(bus.ctx,0x24,1,false)==TP_BUSY);
    fake_i2c.ISR=I2C_ISR_STOPF; assert(bus.read_start(bus.ctx,0x24,1,false)==TP_OK);
    fake_i2c.ISR=I2C_ISR_RXNE; fake_i2c.RXDR=1; assert(bus.read_poll(bus.ctx,out,1)==TP_BUSY);
    fake_i2c.ISR=I2C_ISR_TCR; assert(bus.read_poll(bus.ctx,out,1)==TP_OK && out[0]==1);
    assert(bus.read_continue(bus.ctx,257,true)==TP_OK);
    for(size_t n=0;n<255;n++) {
        fake_i2c.RXDR=data[n]; fake_i2c.ISR=I2C_ISR_RXNE; assert(bus.read_poll(bus.ctx,out,257)==TP_BUSY);
    }
    fake_i2c.ISR=I2C_ISR_TCR; assert(bus.read_poll(bus.ctx,out,257)==TP_BUSY);
    assert(((fake_i2c.CR2>>16)&255u)==2u && (fake_i2c.CR2 & I2C_CR2_AUTOEND));
    for(size_t n=255;n<257;n++) {
        fake_i2c.RXDR=data[n]; fake_i2c.ISR=I2C_ISR_RXNE; assert(bus.read_poll(bus.ctx,out,257)==TP_BUSY);
    }
    fake_i2c.ISR=I2C_ISR_STOPF; assert(bus.read_poll(bus.ctx,out,257)==TP_OK && !memcmp(data,out,257));
    fake_i2c.ISR=0; assert(bus.write_start(bus.ctx,0x24,data,1)==TP_OK);
    fake_i2c.ISR=I2C_ISR_NACKF; assert(bus.write_poll(bus.ctx)==TP_IO_ERROR);
    bus.abort(bus.ctx); assert(fake_i2c.CR1==0 && i.state==TP_F030_I2C_FAILED);
    return 0;
}
