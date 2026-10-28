#include "tp_subordinate.h"
#include <string.h>

#define REQUEST 0x51u
#define RESPONSE 0x52u
#define VERSION 1u
#define FAULT_MASK (TP_SUB_REAGENT_FAULT|TP_SUB_LEVEL_FAULT|TP_SUB_DEVICE_FAULT)
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffffu;
    while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1u)?0xa001u:0)); }
    return c;
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t value) {
    for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(value>>(8*i));
}
static void seal(uint8_t *p,size_t n) {
    uint16_t c=crc(p,n-2); p[n-2]=(uint8_t)c; p[n-1]=(uint8_t)(c>>8);
}
static bool valid(const uint8_t *p,size_t n,uint8_t type) {
    return p && p[0]==type && p[1]==VERSION &&
        crc(p,n-2)==(uint16_t)((uint16_t)p[n-2]|((uint16_t)p[n-1]<<8));
}
tp_result tp_sub_master_init(tp_sub_master *m,uint32_t timeout,uint32_t freshness,uint32_t first) {
    if(!m || !timeout || timeout>0x7fffffffu || !freshness || freshness>0x7fffffffu) return TP_INVALID;
    memset(m,0,sizeof(*m)); m->response_timeout_ms=timeout; m->freshness_ms=freshness; m->next_token=first;
    return TP_OK;
}
void tp_sub_master_tick(tp_sub_master *m,uint32_t now) {
    if(!m) return;
    if(m->pending && (uint32_t)(now-m->deadline_start)>=m->response_timeout_ms) m->pending=false;
    if(m->have_status && (uint32_t)(now-m->last_success)>=m->freshness_ms) m->have_status=false;
}
tp_result tp_sub_master_request(tp_sub_master *m,uint32_t now,uint8_t out[TP_SUB_REQUEST_SIZE]) {
    if(!m || !out || !m->response_timeout_ms || !m->freshness_ms) return TP_INVALID;
    tp_sub_master_tick(m,now);
    if(m->pending) return TP_BUSY;
    out[0]=REQUEST; out[1]=VERSION; put32(out+2,m->next_token);
    m->outstanding_token=m->next_token++; m->deadline_start=now; m->pending=true;
    seal(out,TP_SUB_REQUEST_SIZE); return TP_OK;
}
tp_result tp_sub_master_receive(tp_sub_master *m,uint32_t now,const uint8_t *frame,size_t n) {
    if(!m || !frame || n!=TP_SUB_RESPONSE_SIZE) return TP_INVALID;
    tp_sub_master_tick(m,now);
    if(!m->pending || !valid(frame,n,RESPONSE) || get32(frame+2)!=m->outstanding_token ||
       (frame[6]&~FAULT_MASK) || frame[7] || frame[12] || frame[13]) return TP_INVALID;
    m->status.faults=frame[6]; m->status.droplet_count=get32(frame+8);
    m->last_success=now; m->have_status=true; m->pending=false; return TP_OK;
}
bool tp_sub_master_healthy(const tp_sub_master *m,uint32_t now) {
    return m && m->have_status && (uint32_t)(now-m->last_success)<m->freshness_ms && !m->status.faults;
}
uint16_t tp_sub_master_resource_faults(const tp_sub_master *m,uint32_t now) {
    if(!m || !m->have_status || (uint32_t)(now-m->last_success)>=m->freshness_ms)
        return TP_PREFLIGHT_RESOURCE_UNKNOWN;
    uint16_t result=0;
    if(m->status.faults&TP_SUB_REAGENT_FAULT) result|=TP_PREFLIGHT_REAGENT;
    if(m->status.faults&TP_SUB_LEVEL_FAULT) result|=TP_PREFLIGHT_LEVEL;
    if(m->status.faults&TP_SUB_DEVICE_FAULT) result|=TP_PREFLIGHT_RESOURCE_UNKNOWN;
    return result;
}
tp_result tp_sub_reply(const uint8_t *request,size_t n,const tp_sub_status *s,uint8_t out[TP_SUB_RESPONSE_SIZE]) {
    if(!request || n!=TP_SUB_REQUEST_SIZE || !s || !out || (s->faults&~FAULT_MASK) ||
       !valid(request,n,REQUEST)) return TP_INVALID;
    uint32_t token=get32(request+2);
    memset(out,0,TP_SUB_RESPONSE_SIZE); out[0]=RESPONSE; out[1]=VERSION; put32(out+2,token);
    out[6]=s->faults; put32(out+8,s->droplet_count); seal(out,TP_SUB_RESPONSE_SIZE); return TP_OK;
}
