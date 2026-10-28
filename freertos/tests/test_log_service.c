#include "tp_service.h"
#include "tp_modbus.h"
#include <assert.h>
#include <stdio.h>
typedef struct { unsigned calls; bool latest; uint32_t sequence; tp_result result; } stub;
static tp_result request(void *p,bool latest,uint32_t seq) {
    stub *s=p; s->calls++; s->latest=latest; s->sequence=seq; return s->result;
}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
int main(void) {
    tp_controller c={0}; tp_step recipe[]={ {.kind=TP_STEP_END} }; tp_service s; stub q={0};
    assert(tp_service_init(&s,&c,recipe,1,0)==TP_OK);
    uint16_t words[12]; assert(tp_service_read_holding(&s,53,12,words)==TP_OK && words[0]==0);
    assert(tp_service_attach_log(&s,&q,request)==TP_OK);
    tp_modbus port={&s,1,tp_service_read_holding,tp_service_write_holding};
    uint8_t frame[]={1,16,0,50,0,3,6,0,1,0x12,0x34,0x56,0x78,0,0};
    uint16_t check=crc(frame,13); frame[13]=(uint8_t)check; frame[14]=(uint8_t)(check>>8);
    uint8_t response[64]; size_t n;
    assert(tp_modbus_frame(&port,frame,sizeof(frame),response,sizeof(response),&n)==TP_OK && response[1]==16);
    assert(q.calls==1 && !q.latest && q.sequence==0x12345678u && s.log_status==2);
    uint16_t latest[]={0,0,0}; assert(tp_service_write_holding(&s,50,3,latest)==TP_BUSY);
    tp_event event={.sequence=0x12345678u,.type=3,.detail=4,.utc_valid=true,.utc_seconds=-1,.value=0xaabbccddu};
    tp_service_log_complete(&s,TP_OK,&event);
    assert(tp_service_read_holding(&s,53,12,words)==TP_OK);
    assert(words[0]==3 && words[1]==0x1234 && words[2]==0x5678 && words[3]==3 && words[4]==4 && words[5]==1);
    for(int i=6;i<10;i++) assert(words[i]==0xffff);
    assert(words[10]==0xaabb && words[11]==0xccdd);
    assert(tp_service_write_holding(&s,50,3,latest)==TP_OK && q.latest && s.log_status==2);
    tp_service_log_complete(&s,TP_INVALID,NULL); assert(s.log_status==4);
    assert(tp_service_read_holding(&s,53,12,words)==TP_OK && words[2]==0 && words[10]==0);
    latest[2]=1; assert(tp_service_write_holding(&s,50,3,latest)==TP_INVALID); latest[2]=0;
    q.result=TP_BUSY; assert(tp_service_write_holding(&s,50,3,latest)==TP_BUSY && s.log_status==4);
    q.result=TP_OK; assert(tp_service_write_holding(&s,50,3,latest)==TP_OK);
    tp_service_log_complete(&s,TP_IO_ERROR,NULL); assert(s.log_status==5);
    puts("asynchronous log protocol checks passed"); return 0;
}
