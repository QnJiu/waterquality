#include "tp_freemodbus_frame.h"
#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static void seal(uint8_t *q,size_t n) { uint16_t c=crc(q,n-2); q[n-2]=(uint8_t)c; q[n-1]=(uint8_t)(c>>8); }
int main(void) {
    tp_controller c={0}; tp_service s; tp_step recipe[]={ {.kind=TP_STEP_END} };
    assert(tp_service_init(&s,&c,recipe,1,0)==TP_OK);
    tp_modbus a={&s,1,tp_service_read_holding,tp_service_write_holding};
    tp_modbus b={&s,2,tp_service_read_holding,tp_service_write_holding};
    uint8_t response[256]; size_t n;
    uint8_t read[]={1,3,0,10,0,2,0,0}; seal(read,sizeof(read));
    assert(tp_freemodbus_frame(&a,read,sizeof(read),response,sizeof(response),&n)==TP_OK);
    assert(n==9 && response[0]==1 && response[1]==3 && response[4]==TP_IDLE && crc(response,n)==0);
    assert(tp_freemodbus_frame(&b,read,sizeof(read),response,sizeof(response),&n)==TP_BUSY && n==0);
    read[0]=2; seal(read,sizeof(read));
    assert(tp_freemodbus_frame(&b,read,sizeof(read),response,sizeof(response),&n)==TP_OK && response[0]==2);
    uint8_t write[]={0,16,0,1,0,5,10,0,1,0,0,0,10,0,0,0,0,0,0}; seal(write,sizeof(write));
    assert(tp_freemodbus_frame(&a,write,sizeof(write),response,sizeof(response),&n)==TP_BUSY && n==0);
    assert(s.schedule.mode==TP_MODE_PERIODIC && s.schedule.period_ms==10000);
    read[0]=1; read[3]=200; seal(read,sizeof(read));
    assert(tp_freemodbus_frame(&a,read,sizeof(read),response,sizeof(response),&n)==TP_OK);
    assert(n==5 && response[1]==0x83 && response[2]==2 && crc(response,n)==0);
    read[7]^=1; assert(tp_freemodbus_frame(&a,read,sizeof(read),response,sizeof(response),&n)==TP_INVALID && n==0);
    write[0]=1; seal(write,sizeof(write)); s.schedule.mode=TP_MODE_CONTROLLED;
    assert(tp_freemodbus_frame(&a,write,sizeof(write),response,5,&n)==TP_INVALID);
    assert(s.schedule.mode==TP_MODE_CONTROLLED);
    puts("FreeModbus frame adapter checks passed"); return 0;
}
