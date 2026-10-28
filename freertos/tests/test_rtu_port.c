#include "tp_rtu_port.h"
#include "tp_modbus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { tp_modbus modbus[2]; unsigned received[2], replies[2], errors[2]; bool busy; } fake;
static tp_result read_regs(void *ctx, uint16_t a, uint16_t n, uint16_t *v) {
    (void)ctx; if (a || n!=1) return TP_INVALID; v[0]=0x1234; return TP_OK;
}
static tp_result submit(void *ctx, unsigned port, const uint8_t *q, size_t len) {
    fake *f=ctx; assert(port<2); f->received[port]++;
    if (f->busy) return TP_BUSY;
    uint8_t out[256]; size_t size=0;
    if (tp_modbus_frame(&f->modbus[port],q,len,out,sizeof(out),&size)==TP_OK) {
        assert(size==7 && out[0]==port+1 && out[3]==0x12 && out[4]==0x34);
        f->replies[port]++;
    }
    return TP_OK;
}
static void error(void *ctx, unsigned port, tp_result r) { fake *f=ctx; assert(r==TP_BUSY || r==TP_IO_ERROR); f->errors[port]++; }
static void crc(uint8_t *b) {
    uint16_t c=0xffff; for (int i=0;i<6;i++) { c^=b[i]; for (int j=0;j<8;j++) c=(c>>1)^((c&1)?0xa001:0); }
    b[6]=(uint8_t)c; b[7]=(uint8_t)(c>>8);
}
static void send_frame(tp_rtu_port *p, uint8_t *b, uint32_t start) {
    for (unsigned i=0;i<8;i++) tp_rtu_port_byte(p,b[i],start+i*1000);
    tp_rtu_port_poll(p,start+7000+p->receiver.gap_3_5_us+p->receiver.char_us);
}
int main(void) {
    fake f={0}; tp_rtu_port p[2];
    f.modbus[0]=(tp_modbus){.address=1,.read_holding=read_regs};
    f.modbus[1]=(tp_modbus){.address=2,.read_holding=read_regs};
    assert(tp_rtu_port_init(&p[0],0,9600,11,&f,submit,error)==TP_OK);
    assert(tp_rtu_port_init(&p[1],1,19200,11,&f,submit,error)==TP_OK);
    uint8_t b[8]={1,3,0,0,0,1}; crc(b);
    send_frame(&p[0],b,0); assert(f.replies[0]==1 && f.replies[1]==0);
    b[0]=2; crc(b); send_frame(&p[1],b,20000); assert(f.replies[1]==1);
    b[7]^=1; send_frame(&p[1],b,40000); assert(f.received[1]==2 && f.replies[1]==1);
    tp_rtu_port_byte(&p[0],1,60000); tp_rtu_port_error(&p[0],61000);
    tp_rtu_port_poll(&p[0],61000+p[0].receiver.gap_3_5_us+p[0].receiver.char_us);
    assert(f.received[0]==1 && f.errors[0]==1);
    f.busy=true; b[0]=1; crc(b); send_frame(&p[0],b,80000);
    assert(f.received[0]==2 && f.errors[0]==2 && f.errors[1]==0);
    /* Two UART tasks can deliver bytes interleaved; an error on one must not
     * disturb the frame being assembled on the other. */
    fake concurrent={0}; tp_rtu_port ports[2];
    concurrent.modbus[0]=(tp_modbus){.address=1,.read_holding=read_regs};
    concurrent.modbus[1]=(tp_modbus){.address=2,.read_holding=read_regs};
    assert(tp_rtu_port_init(&ports[0],0,9600,11,&concurrent,submit,error)==TP_OK);
    assert(tp_rtu_port_init(&ports[1],1,19200,11,&concurrent,submit,error)==TP_OK);
    uint8_t first[8]={1,3,0,0,0,1}, second[8]={2,3,0,0,0,1};
    crc(first); crc(second);
    for (unsigned cycle=0;cycle<100;cycle++) {
        uint32_t start=cycle*20000;
        for (unsigned i=0;i<8;i++) {
            tp_rtu_port_byte(&ports[0],first[i],start+i*1000);
            tp_rtu_port_byte(&ports[1],second[i],start+i*1000);
        }
        tp_rtu_port_poll(&ports[1],start+14000);
        tp_rtu_port_poll(&ports[0],start+14000);
    }
    assert(concurrent.replies[0]==100 && concurrent.replies[1]==100);
    for (unsigned i=0;i<8;i++) {
        tp_rtu_port_byte(&ports[0],first[i],2000000+i*1000);
        tp_rtu_port_byte(&ports[1],second[i],2000000+i*1000);
        if (i==3) tp_rtu_port_error(&ports[0],2000000+i*1000);
    }
    tp_rtu_port_poll(&ports[0],2014000);
    tp_rtu_port_poll(&ports[1],2014000);
    assert(concurrent.errors[0]==1 && concurrent.errors[1]==0);
    assert(concurrent.replies[0]==100 && concurrent.replies[1]==101);
    puts("RTU port integration checks passed"); return 0;
}
