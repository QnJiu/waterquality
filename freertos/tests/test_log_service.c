#include "tp_service.h"
#include "tp_modbus.h"
#include "tp_event_transition.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef struct { unsigned calls; bool latest; uint32_t sequence; tp_result result; } stub;
static tp_result request(void *p,bool latest,uint32_t seq) {
    stub *s=p; s->calls++; s->latest=latest; s->sequence=seq; return s->result;
}
static void stats_snapshot(void *p,uint32_t counters[4]) {
    unsigned *calls=p; (*calls)++;
    counters[0]=0x12345678u; counters[1]=3; counters[2]=0x10002u; counters[3]=7;
}
typedef struct { uint8_t bytes[128]; } memory_log;
static tp_result memory_read(void *ctx, uint32_t offset, void *data, size_t size) {
    memory_log *m=ctx; assert(offset+size<=sizeof(m->bytes)); memcpy(data,m->bytes+offset,size); return TP_OK;
}
static tp_result memory_write(void *ctx, uint32_t offset, const void *data, size_t size) {
    memory_log *m=ctx; assert(offset+size<=sizeof(m->bytes)); memcpy(m->bytes+offset,data,size); return TP_OK;
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
    uint16_t stats[9];
    assert(tp_service_read_holding(&s,110,9,stats)==TP_OK && stats[0]==0 && stats[5]==0);
    unsigned snapshots=0;
    assert(tp_service_attach_log_stats(&s,&snapshots,stats_snapshot)==TP_OK);
    assert(tp_service_attach_log_stats(&s,&snapshots,stats_snapshot)==TP_INVALID);
    uint8_t read_stats[]={1,3,0,110,0,9,0,0};
    check=crc(read_stats,6); read_stats[6]=(uint8_t)check; read_stats[7]=(uint8_t)(check>>8);
    assert(tp_modbus_frame(&port,read_stats,sizeof(read_stats),response,sizeof(response),&n)==TP_OK);
    assert(n==23 && response[1]==3 && response[2]==18 && snapshots==1);
    assert(response[3]==0 && response[4]==1 && response[5]==0x12 && response[6]==0x34 &&
           response[7]==0x56 && response[8]==0x78 && response[13]==0 && response[14]==1 &&
           response[15]==0 && response[16]==2 && response[20]==7);
    assert(tp_service_read_holding(&s,115,2,stats)==TP_OK && stats[0]==1 && stats[1]==2 && snapshots==2);
    /* A finished measurement travels through the event journal and the same
     * asynchronous Modbus snapshot used by the teaching GUI. */
    memory_log memory={0};
    tp_event_log_io io={.ctx=&memory,.read=memory_read,.write=memory_write,.size=sizeof(memory.bytes)};
    tp_event_log journal; assert(tp_event_log_open(&journal,&io)==TP_OK);
    c.state=TP_COMPLETE; c.result_valid=true; c.result_mg_l=3.25f;
    assert(tp_event_transition(TP_RUNNING,&c,&event));
    event.utc_valid=true; event.utc_seconds=1700000000;
    assert(tp_event_log_append(&journal,&event)==TP_OK && event.sequence==1);
    /* Reopen using the same persistent bytes, as after a restart. */
    tp_event_log reopened; assert(tp_event_log_open(&reopened,&io)==TP_OK);
    tp_service_log_complete(&s,TP_INVALID,NULL);
    q.result=TP_OK;
    uint8_t query[]={1,16,0,50,0,3,6,0,1,0,0,0,1,0,0};
    check=crc(query,13); query[13]=(uint8_t)check; query[14]=(uint8_t)(check>>8);
    assert(tp_modbus_frame(&port,query,sizeof(query),response,sizeof(response),&n)==TP_OK &&
           response[1]==16 && !q.latest && q.sequence==1);
    tp_event stored; assert(tp_event_log_read(&reopened,q.sequence,&stored)==TP_OK);
    tp_service_log_complete(&s,TP_OK,&stored);
    assert(tp_service_read_holding(&s,53,12,words)==TP_OK);
    uint8_t read_event[]={1,3,0,53,0,12,0,0};
    check=crc(read_event,6); read_event[6]=(uint8_t)check; read_event[7]=(uint8_t)(check>>8);
    assert(tp_modbus_frame(&port,read_event,sizeof(read_event),response,sizeof(response),&n)==TP_OK &&
           n==29 && response[1]==3 && response[2]==24 && response[23]==0x40 && response[24]==0x50);
    assert(words[0]==3 && words[1]==0 && words[2]==1 && words[3]==TP_EVENT_COMPLETED &&
           words[4]==1 && words[5]==1 && words[8]==0x6553 && words[9]==0xf100 &&
           words[10]==0x4050 && words[11]==0);
    puts("asynchronous log protocol checks passed"); return 0;
}
