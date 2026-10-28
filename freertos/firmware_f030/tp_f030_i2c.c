#include "tp_f030_i2c.h"
#include "stm32f0xx.h"
#include <string.h>

#define ERRORS (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)
#define CLEAR_FLAGS (I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF)

static tp_result fail(tp_f030_i2c *i) { i->state=TP_F030_I2C_FAILED; return TP_IO_ERROR; }
static void segment(tp_f030_i2c *i,bool start,bool reading,uint8_t address) {
    size_t remaining=i->length-i->position;
    uint32_t count=(uint32_t)(remaining>255u ? 255u : remaining);
    bool reload=remaining>255u || (reading && !i->final);
    i->segment_end=i->position+count;
    uint32_t cr2=((uint32_t)address<<1) | (count<<I2C_CR2_NBYTES_Pos);
    if(reading) cr2|=I2C_CR2_RD_WRN;
    if(reload) cr2|=I2C_CR2_RELOAD; else cr2|=I2C_CR2_AUTOEND;
    if(start) cr2|=I2C_CR2_START;
    I2C1->CR2=cr2;
}
static tp_result ready(tp_f030_i2c *i) {
    if(i->state==TP_F030_I2C_STOPPING) {
        if(I2C1->ISR & ERRORS) return fail(i);
        if(!(I2C1->ISR & I2C_ISR_STOPF)) return TP_BUSY;
        I2C1->ICR=I2C_ICR_STOPCF; i->state=TP_F030_I2C_IDLE;
    }
    if(i->state!=TP_F030_I2C_IDLE) return i->state==TP_F030_I2C_FAILED ? TP_IO_ERROR : TP_BUSY;
    return (I2C1->ISR & I2C_ISR_BUSY) ? TP_BUSY : TP_OK;
}
static tp_result write_start(void *ctx,uint8_t address,const uint8_t *data,size_t n) {
    tp_f030_i2c *i=ctx;
    if(!i || !data || !n || n>TP_PN532_FRAME_MAX || address>0x7f) return TP_INVALID;
    tp_result r=ready(i); if(r!=TP_OK) return r;
    i->write_data=data; i->length=n; i->position=0; i->state=TP_F030_I2C_WRITE;
    I2C1->ICR=CLEAR_FLAGS; segment(i,true,false,address); return TP_OK;
}
static tp_result write_poll(void *ctx) {
    tp_f030_i2c *i=ctx;
    if(!i || i->state!=TP_F030_I2C_WRITE) return TP_INVALID;
    uint32_t flags=I2C1->ISR;
    if(flags & ERRORS) return fail(i);
    if((flags & I2C_ISR_TXIS) && i->position<i->segment_end)
        I2C1->TXDR=i->write_data[i->position++];
    if((flags & I2C_ISR_TCR) && i->position==i->segment_end && i->position<i->length)
        segment(i,false,false,(uint8_t)((I2C1->CR2 & I2C_CR2_SADD)>>1));
    if(flags & I2C_ISR_STOPF) {
        I2C1->ICR=I2C_ICR_STOPCF;
        if(i->position!=i->length) return fail(i);
        i->state=TP_F030_I2C_IDLE; return TP_OK;
    }
    return TP_BUSY;
}
static tp_result read_start(void *ctx,uint8_t address,size_t n,bool final) {
    tp_f030_i2c *i=ctx;
    if(!i || !n || n>sizeof(i->read_data) || address>0x7f) return TP_INVALID;
    tp_result r=ready(i); if(r!=TP_OK) return r;
    i->length=n; i->position=0; i->final=final; i->state=TP_F030_I2C_READ;
    I2C1->ICR=CLEAR_FLAGS; segment(i,true,true,address); return TP_OK;
}
static tp_result read_continue(void *ctx,size_t n,bool final) {
    tp_f030_i2c *i=ctx;
    if(!i || i->state!=TP_F030_I2C_HELD || !n || n>sizeof(i->read_data) ||
       !(I2C1->ISR & I2C_ISR_TCR)) return TP_INVALID;
    i->length=n; i->position=0; i->final=final; i->state=TP_F030_I2C_READ;
    segment(i,false,true,(uint8_t)((I2C1->CR2 & I2C_CR2_SADD)>>1)); return TP_OK;
}
static tp_result read_poll(void *ctx,uint8_t *out,size_t n) {
    tp_f030_i2c *i=ctx;
    if(!i || !out || i->state!=TP_F030_I2C_READ || n!=i->length) return TP_INVALID;
    uint32_t flags=I2C1->ISR;
    if(flags & ERRORS) return fail(i);
    if((flags & I2C_ISR_RXNE) && i->position<i->segment_end)
        i->read_data[i->position++]=(uint8_t)I2C1->RXDR;
    if((flags & I2C_ISR_TCR) && i->position==i->segment_end && i->position<i->length)
        segment(i,false,true,(uint8_t)((I2C1->CR2 & I2C_CR2_SADD)>>1));
    if(i->position==i->length && ((i->final && (flags & I2C_ISR_STOPF)) ||
                                  (!i->final && (flags & I2C_ISR_TCR)))) {
        if(i->final) { I2C1->ICR=I2C_ICR_STOPCF; i->state=TP_F030_I2C_IDLE; }
        else i->state=TP_F030_I2C_HELD;
        memcpy(out,i->read_data,n); return TP_OK;
    }
    if(flags & I2C_ISR_STOPF) return fail(i);
    return TP_BUSY;
}
static void read_stop(void *ctx) {
    tp_f030_i2c *i=ctx;
    if(i && i->state==TP_F030_I2C_HELD) { I2C1->CR2|=I2C_CR2_STOP; i->state=TP_F030_I2C_STOPPING; }
}
static void abort_bus(void *ctx) {
    tp_f030_i2c *i=ctx;
    if(i) { I2C1->CR1=0; I2C1->ICR=CLEAR_FLAGS; i->state=TP_F030_I2C_FAILED; }
}
tp_result tp_f030_i2c_init(tp_f030_i2c *i) {
    if(!i || SystemCoreClock!=8000000u || (RCC->CFGR3 & RCC_CFGR3_I2C1SW)) return TP_INVALID;
    memset(i,0,sizeof(*i));
    RCC->AHBENR|=RCC_AHBENR_GPIOBEN; RCC->APB1ENR|=RCC_APB1ENR_I2C1EN;
    (void)RCC->AHBENR; (void)RCC->APB1ENR;
    GPIOB->OTYPER|=(1u<<6)|(1u<<7);
    GPIOB->PUPDR&=~((3u<<12)|(3u<<14));
    GPIOB->AFR[0]=(GPIOB->AFR[0]&~((15u<<24)|(15u<<28))) | (1u<<24)|(1u<<28);
    GPIOB->MODER=(GPIOB->MODER&~((3u<<12)|(3u<<14))) | (2u<<12)|(2u<<14);
    I2C1->CR1=0; I2C1->CR2=0; I2C1->TIMINGR=0x10420f13u;
    I2C1->ICR=CLEAR_FLAGS; I2C1->CR1=I2C_CR1_PE;
    return TP_OK;
}
tp_pn532_i2c_bus tp_f030_i2c_bus(tp_f030_i2c *i) {
    tp_pn532_i2c_bus b={i,write_start,write_poll,read_start,read_continue,read_poll,read_stop,abort_bus};
    return b;
}
