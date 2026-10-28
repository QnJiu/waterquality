#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Deterministic single-threaded port used only by this host test. */
static eMBEventType events[8]; static unsigned head, tail;
static UCHAR rx, tx[256]; static unsigned tx_len; static BOOL transmit;
static tp_service *service;
BOOL xMBPortEventInit(void) { head=tail=0; return TRUE; }
BOOL xMBPortEventPost(eMBEventType event) { assert(tail-head<8); events[tail++%8]=event; return TRUE; }
BOOL xMBPortEventGet(eMBEventType *event) { if(head==tail) return FALSE; *event=events[head++%8]; return TRUE; }
BOOL xMBPortSerialInit(UCHAR port,ULONG baud,UCHAR bits,eMBParity parity,UCHAR stop) {
    (void)port; (void)baud; (void)bits; (void)parity; (void)stop; return TRUE;
}
void vMBPortSerialEnable(BOOL read,BOOL write) { (void)read; transmit=write; }
BOOL xMBPortSerialGetByte(CHAR *b) { *b=(CHAR)rx; return TRUE; }
BOOL xMBPortSerialPutByte(CHAR b) { assert(tx_len<sizeof(tx)); tx[tx_len++]=(UCHAR)b; return TRUE; }
BOOL xMBPortTimersInit(USHORT ticks) { return ticks>0; }
void vMBPortTimersEnable(void) { }
void vMBPortTimersDisable(void) { }
void vMBPortTimersDelay(USHORT ms) { (void)ms; }
eMBErrorCode eMBRegHoldingCB(UCHAR *buffer,USHORT address,USHORT count,eMBRegisterMode mode) {
    uint16_t words[125];
    /* Upstream presents addresses one-based, with 0 representing wire address 65535. */
    uint16_t start=(uint16_t)(address-1u);
    if (!count || count>125 || (uint32_t)start+count>65536u) return MB_ENOREG;
    tp_result r;
    if (mode==MB_REG_READ) {
        r=tp_service_read_holding(service,start,count,words);
        if(r==TP_OK) for(unsigned i=0;i<count;i++) { buffer[2*i]=(UCHAR)(words[i]>>8); buffer[2*i+1]=(UCHAR)words[i]; }
    } else {
        for(unsigned i=0;i<count;i++) words[i]=(uint16_t)(((uint16_t)buffer[2*i]<<8)|buffer[2*i+1]);
        r=tp_service_write_holding(service,start,count,words);
    }
    return r==TP_OK ? MB_ENOERR : r==TP_INVALID ? MB_ENOREG : MB_EIO;
}
static uint16_t crc(const UCHAR *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static void exchange(UCHAR *frame,size_t len) {
    uint16_t c=crc(frame,len-2); frame[len-2]=(UCHAR)c; frame[len-1]=(UCHAR)(c>>8); tx_len=0;
    for(size_t i=0;i<len;i++) { rx=frame[i]; (void)pxMBFrameCBByteReceived(); }
    (void)pxMBPortCBTimerExpired();
    while(head!=tail) assert(eMBPoll()==MB_ENOERR);
    while(transmit) (void)pxMBFrameCBTransmitterEmpty();
    while(head!=tail) assert(eMBPoll()==MB_ENOERR);
}
int main(void) {
    tp_controller controller={0}; tp_service s; tp_step recipe[]={ {.kind=TP_STEP_END} };
    assert(tp_service_init(&s,&controller,recipe,1,0)==TP_OK); service=&s;
    assert(eMBInit(MB_RTU,1,0,9600,MB_PAR_NONE,1)==MB_ENOERR);
    assert(eMBEnable()==MB_ENOERR); (void)pxMBPortCBTimerExpired();
    while(head!=tail) assert(eMBPoll()==MB_ENOERR);
    UCHAR read[]={1,3,0,10,0,2,0,0}; exchange(read,sizeof(read));
    assert(tx_len==9 && tx[1]==3 && tx[2]==4 && tx[4]==TP_IDLE && crc(tx,tx_len)==0);
    UCHAR invalid[]={1,3,0,200,0,1,0,0}; exchange(invalid,sizeof(invalid));
    assert(tx_len==5 && tx[1]==0x83 && tx[2]==2 && crc(tx,tx_len)==0);
    UCHAR broadcast[]={0,16,0,1,0,5,10,0,1,0,0,0,10,0,0,0,0,0,0};
    exchange(broadcast,sizeof(broadcast)); assert(tx_len==0 && s.schedule.mode==TP_MODE_PERIODIC && s.schedule.period_ms==10000);
    assert(eMBDisable()==MB_ENOERR); puts("FreeModbus RTU stack host checks passed"); return 0;
}
