#include "tp_pn532_i2c.h"
#include <string.h>

static tp_result fault(tp_pn532_i2c *p,tp_result r) {
    p->bus.abort(p->bus.ctx); p->state=TP_PN532_I2C_FAULT;
    return r==TP_BUSY ? TP_IO_ERROR : r;
}
tp_result tp_pn532_i2c_init(tp_pn532_i2c *p,const tp_pn532_i2c_bus *bus) {
    if(!p || !bus || !bus->write_start || !bus->write_poll || !bus->read_start ||
       !bus->read_continue || !bus->read_poll || !bus->read_stop || !bus->abort) return TP_INVALID;
    tp_pn532_i2c_bus copy=*bus; memset(p,0,sizeof(*p)); p->bus=copy; return TP_OK;
}
void tp_pn532_i2c_abort(tp_pn532_i2c *p) {
    if(p && p->bus.abort) { p->bus.abort(p->bus.ctx); p->state=TP_PN532_I2C_FAULT; }
}
static tp_result send(void *ctx,const uint8_t *frame,size_t n) {
    tp_pn532_i2c *p=ctx;
    if(!p || !frame || !n || n>sizeof(p->frame)) return TP_INVALID;
    if(p->state!=TP_PN532_I2C_IDLE) return TP_BUSY;
    memcpy(p->frame,frame,n); p->length=n;
    tp_result r=p->bus.write_start(p->bus.ctx,TP_PN532_I2C_ADDRESS,p->frame,n);
    if(r!=TP_OK) return fault(p,r);
    p->state=TP_PN532_I2C_WRITE; return TP_OK;
}
static tp_result receive(void *ctx,uint8_t *frame,size_t cap,size_t *len) {
    tp_pn532_i2c *p=ctx;
    if(!p || !frame || !len || p->state==TP_PN532_I2C_FAULT) return TP_INVALID;
    tp_result r;
    if(p->state==TP_PN532_I2C_WRITE) {
        r=p->bus.write_poll(p->bus.ctx);
        if(r==TP_BUSY) return r;
        if(r!=TP_OK) return fault(p,r);
        p->state=TP_PN532_I2C_IDLE;
    }
    if(p->state==TP_PN532_I2C_IDLE) {
        r=p->bus.read_start(p->bus.ctx,TP_PN532_I2C_ADDRESS,1,false);
        if(r==TP_BUSY) return r;
        if(r!=TP_OK) return fault(p,r);
        p->state=TP_PN532_I2C_STATUS; return TP_BUSY;
    }
    if(p->state==TP_PN532_I2C_STATUS) {
        uint8_t status;
        r=p->bus.read_poll(p->bus.ctx,&status,1);
        if(r==TP_BUSY) return r;
        if(r!=TP_OK) return fault(p,r);
        if(!(status&1u)) { p->bus.read_stop(p->bus.ctx); p->state=TP_PN532_I2C_IDLE; return TP_BUSY; }
        r=p->bus.read_continue(p->bus.ctx,5,false);
        if(r!=TP_OK) return fault(p,r);
        p->state=TP_PN532_I2C_HEADER; return TP_BUSY;
    }
    if(p->state==TP_PN532_I2C_HEADER) {
        r=p->bus.read_poll(p->bus.ctx,p->frame,5);
        if(r==TP_BUSY) return r;
        if(r!=TP_OK) return fault(p,r);
        if(p->frame[0] || p->frame[1] || p->frame[2]!=0xff ||
           (p->frame[3]==0 ? p->frame[4]!=0xff :
            (uint8_t)(p->frame[3]+p->frame[4])!=0)) return fault(p,TP_INVALID);
        p->remaining=p->frame[3] ? (size_t)p->frame[3]+2u : 1u;
        p->length=5u+p->remaining;
        if(p->length>sizeof(p->frame) || p->length>cap) return fault(p,TP_INVALID);
        r=p->bus.read_continue(p->bus.ctx,p->remaining,true);
        if(r!=TP_OK) return fault(p,r);
        p->state=TP_PN532_I2C_BODY; return TP_BUSY;
    }
    if(p->state!=TP_PN532_I2C_BODY) return fault(p,TP_INVALID);
    r=p->bus.read_poll(p->bus.ctx,p->frame+5,p->remaining);
    if(r==TP_BUSY) return r;
    if(r!=TP_OK) return fault(p,r);
    if(cap<p->length) return fault(p,TP_INVALID);
    memcpy(frame,p->frame,p->length); *len=p->length;
    p->state=TP_PN532_I2C_IDLE; return TP_OK;
}
tp_pn532_io tp_pn532_i2c_io(tp_pn532_i2c *p) {
    tp_pn532_io io={p,send,receive}; return io;
}
