#include "tp_modbus.h"
#define MAX_REGS 123u
static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static uint16_t crc(const uint8_t *p, size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static tp_result reply(uint8_t *b, size_t n, size_t cap, size_t *out) {
    if(n+2>cap) return TP_INVALID;
    uint16_t c=crc(b,n); b[n]=(uint8_t)c; b[n+1]=(uint8_t)(c>>8); *out=n+2; return TP_OK;
}
static tp_result exception(uint8_t *b,size_t cap,size_t *out,uint8_t unit,uint8_t fn,uint8_t ex,bool broadcast) {
    if(broadcast) return TP_BUSY;
    if(cap<5) return TP_INVALID;
    b[0]=unit; b[1]=(uint8_t)(fn|0x80); b[2]=ex; return reply(b,3,cap,out);
}
tp_result tp_modbus_frame(tp_modbus *m,const uint8_t *q,size_t len,uint8_t *b,size_t cap,size_t *out) {
    if(!m || !q || !b || !out || m->address==0 || m->address>247) return TP_INVALID;
    *out=0; if(len<4 || len>256 || crc(q,len-2)!=(uint16_t)(q[len-2]|((uint16_t)q[len-1]<<8))) return TP_INVALID;
    bool broadcast=q[0]==0; if(q[0]!=m->address && !broadcast) return TP_BUSY;
    uint8_t fn=q[1]; if(fn!=3 && fn!=6 && fn!=16) return exception(b,cap,out,m->address,fn,1,broadcast);
    if(broadcast && fn==3) return TP_BUSY;
    if(len<8) return exception(b,cap,out,m->address,fn,3,broadcast);
    uint16_t addr=be16(q+2), count=(fn==6)?1:be16(q+4); uint16_t values[MAX_REGS];
    if(count==0 || (uint32_t)addr+count>65536u || (fn==3 && count>125) ||
       (fn==16 && count>MAX_REGS)) return exception(b,cap,out,m->address,fn,3,broadcast);
    if((fn==3 || fn==6) && len!=8) return exception(b,cap,out,m->address,fn,3,broadcast);
    if(fn==16 && (len<9 || q[6]!=count*2u || len!=(size_t)(9u+count*2u)))
        return exception(b,cap,out,m->address,fn,3,broadcast);
    if(fn==3) {
        /* FC03 allows 125 registers, so read directly into a separate full-size buffer. */
        uint16_t read_values[125]; if(!m->read_holding) return exception(b,cap,out,m->address,fn,1,false);
        if(cap<(size_t)(5u+count*2u)) return TP_INVALID;
        tp_result r=m->read_holding(m->ctx,addr,count,read_values);
        if(r!=TP_OK) return exception(b,cap,out,m->address,fn,r==TP_INVALID?2:4,false);
        b[0]=m->address; b[1]=fn; b[2]=(uint8_t)(count*2u);
        for(unsigned i=0;i<count;i++) put16(b+3+2*i,read_values[i]);
        return reply(b,3+count*2u,cap,out);
    }
    if(!broadcast && cap<8) return TP_INVALID;
    if(!m->write_holding) return exception(b,cap,out,m->address,fn,1,broadcast);
    if(fn==6) values[0]=be16(q+4); else for(unsigned i=0;i<count;i++) values[i]=be16(q+7+2*i);
    tp_result r=m->write_holding(m->ctx,addr,count,values);
    if(r!=TP_OK) return exception(b,cap,out,m->address,fn,r==TP_INVALID?2:4,broadcast);
    if(broadcast) return TP_BUSY;
    b[0]=m->address; b[1]=fn; put16(b+2,addr); put16(b+4,fn==6?values[0]:count); return reply(b,6,cap,out);
}

