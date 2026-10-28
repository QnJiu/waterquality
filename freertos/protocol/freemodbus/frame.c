#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "tp_freemodbus_frame.h"
#include <string.h>

/* All functions here execute synchronously in the one control task; RTU line
 * timing is handled by tp_rtu_receiver before a complete frame reaches us. */
static tp_modbus *current;
static eMBEventType events[8];
static unsigned head, tail;
static UCHAR input, output[256];
static size_t output_len;
static BOOL transmitting, port_error;
BOOL xMBPortEventInit(void) { head=tail=0; return TRUE; }
BOOL xMBPortEventPost(eMBEventType event) {
    if (tail-head>=8) { port_error=TRUE; return FALSE; }
    events[tail++%8]=event; return TRUE;
}
BOOL xMBPortEventGet(eMBEventType *event) {
    if(head==tail) return FALSE; *event=events[head++%8]; return TRUE;
}
BOOL xMBPortSerialInit(UCHAR port,ULONG baud,UCHAR bits,eMBParity parity,UCHAR stop) {
    (void)port; (void)baud; (void)bits; (void)parity; (void)stop; return TRUE;
}
void vMBPortSerialEnable(BOOL rx,BOOL tx) { (void)rx; transmitting=tx; }
BOOL xMBPortSerialGetByte(CHAR *b) { *b=(CHAR)input; return TRUE; }
BOOL xMBPortSerialPutByte(CHAR b) {
    if(output_len==sizeof(output)) { port_error=TRUE; return FALSE; }
    output[output_len++]=(UCHAR)b; return TRUE;
}
BOOL xMBPortTimersInit(USHORT ticks) { return ticks>0; }
void vMBPortTimersEnable(void) { }
void vMBPortTimersDisable(void) { }
void vMBPortTimersDelay(USHORT ms) { (void)ms; }
eMBErrorCode eMBRegHoldingCB(UCHAR *buffer,USHORT address,USHORT count,eMBRegisterMode mode) {
    uint16_t words[125]; uint16_t start=(uint16_t)(address-1u);
    if (!current || !count || count>125 || (uint32_t)start+count>65536u) return MB_ENOREG;
    tp_result r;
    if (mode==MB_REG_READ) {
        if(!current->read_holding) return MB_EIO;
        r=current->read_holding(current->ctx,start,count,words);
        if(r==TP_OK) for(unsigned i=0;i<count;i++) { buffer[2*i]=(UCHAR)(words[i]>>8); buffer[2*i+1]=(UCHAR)words[i]; }
    } else {
        if(!current->write_holding) return MB_EIO;
        for(unsigned i=0;i<count;i++) words[i]=(uint16_t)(((uint16_t)buffer[2*i]<<8)|buffer[2*i+1]);
        r=current->write_holding(current->ctx,start,count,words);
    }
    return r==TP_OK ? MB_ENOERR : r==TP_INVALID ? MB_ENOREG : MB_EIO;
}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static bool drain_events(void) {
    unsigned budget=8;
    while(head!=tail && budget--) if(eMBPoll()!=MB_ENOERR) return false;
    return head==tail && !port_error;
}
tp_result tp_freemodbus_frame(tp_modbus *m,const uint8_t *q,size_t n,uint8_t *response,size_t cap,size_t *out) {
    if(!m || !q || !response || !out || !m->address || m->address>247) return TP_INVALID;
    *out=0;
    if(n<4 || n>256 || crc(q,n)!=0) return TP_INVALID;
    if(q[0]!=0 && q[0]!=m->address) return TP_BUSY;
    if(q[0]==0 && q[1]==3) return TP_BUSY;
    /* Avoid changing application state if a successful write cannot be returned. */
    if(q[0]!=0 && (q[1]==6 || q[1]==16) && cap<8) return TP_INVALID;
    if(cap<5) return TP_INVALID;
    if(current) return TP_BUSY; /* caller must serialize all instances */
    current=m; output_len=0; transmitting=FALSE; port_error=FALSE;
    tp_result result=TP_IO_ERROR;
    if(eMBInit(MB_RTU,m->address,0,9600,MB_PAR_NONE,1)!=MB_ENOERR || eMBEnable()!=MB_ENOERR) goto done;
    (void)pxMBPortCBTimerExpired();
    if(!drain_events()) goto disabled;
    for(size_t i=0;i<n;i++) { input=q[i]; (void)pxMBFrameCBByteReceived(); }
    (void)pxMBPortCBTimerExpired();
    if(!drain_events()) goto disabled;
    for(unsigned i=0;transmitting && i<257;i++) (void)pxMBFrameCBTransmitterEmpty();
    if(transmitting || !drain_events()) goto disabled;
    if(!output_len) { result=TP_BUSY; goto disabled; }
    if(output_len>cap) { result=TP_INVALID; goto disabled; }
    memcpy(response,output,output_len); *out=output_len; result=TP_OK;
disabled:
    (void)eMBDisable();
done:
    current=NULL; return result;
}
