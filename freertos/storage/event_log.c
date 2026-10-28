#include "tp_event_log.h"
#include <string.h>
#define MAGIC 0x314c5054u
#define COMMIT 0xa5u
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t crc32(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(c>>1)^(0xedb88320u & (uint32_t)-(int32_t)(c&1u)); } return ~c;
}
static bool decode(const uint8_t *b,tp_event *e) {
    if (b[31]!=COMMIT || get32(b)!=MAGIC || b[24]>1 || b[25]!=0 || b[26]!=0 ||
        get32(b+27)!=crc32(b,27)) return false;
    uint64_t t=(uint64_t)get32(b+8)|((uint64_t)get32(b+12)<<32);
    e->sequence=get32(b+4); memcpy(&e->utc_seconds,&t,sizeof(t));
    e->value=get32(b+16); e->type=(uint16_t)(b[20]|((uint16_t)b[21]<<8));
    e->detail=(uint16_t)(b[22]|((uint16_t)b[23]<<8)); e->utc_valid=b[24]!=0;
    return e->type!=0;
}
static void encode(uint8_t *b,const tp_event *e,uint32_t seq) {
    uint64_t t; memcpy(&t,&e->utc_seconds,sizeof(t)); memset(b,0,32);
    put32(b,MAGIC); put32(b+4,seq); put32(b+8,(uint32_t)t); put32(b+12,(uint32_t)(t>>32));
    put32(b+16,e->value); b[20]=(uint8_t)e->type; b[21]=(uint8_t)(e->type>>8);
    b[22]=(uint8_t)e->detail; b[23]=(uint8_t)(e->detail>>8); b[24]=e->utc_valid ? 1 : 0;
    put32(b+27,crc32(b,27)); b[31]=COMMIT;
}
static bool newer(uint32_t a,uint32_t b) { uint32_t d=a-b; return d!=0 && d<0x80000000u; }
tp_result tp_event_log_open(tp_event_log *log,const tp_event_log_io *io) {
    if (!log || !io || !io->read || !io->write || io->size<64 || io->size%32) return TP_INVALID;
    tp_event_log_io copy=*io; memset(log,0,sizeof(*log)); log->io=copy; log->slots=copy.size/32;
    for(uint32_t i=0;i<log->slots;i++) {
        uint8_t b[32]; tp_event e;
        if(copy.read(copy.ctx,i*32,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
        if(decode(b,&e) && (!log->has_records || newer(e.sequence,log->latest_sequence))) {
            log->has_records=true; log->latest_sequence=e.sequence; log->latest_slot=i;
        }
    }
    log->open=true; return TP_OK;
}
tp_result tp_event_log_append(tp_event_log *log,tp_event *e) {
    if(!log || !log->open || !e || !e->type) return TP_INVALID;
    uint32_t slot=log->has_records ? (log->latest_slot+1)%log->slots : 0;
    uint32_t seq=log->has_records ? log->latest_sequence+1 : 1; uint32_t off=slot*32;
    uint8_t b[32], verify[31], marker, invalid=0; encode(b,e,seq);
    /* Invalidate old slot, write and verify data, publish with one commit byte. */
    if(log->io.write(log->io.ctx,off+31,&invalid,1)!=TP_OK ||
       log->io.read(log->io.ctx,off+31,&marker,1)!=TP_OK || marker!=invalid ||
       log->io.write(log->io.ctx,off,b,31)!=TP_OK ||
       log->io.read(log->io.ctx,off,verify,31)!=TP_OK || memcmp(verify,b,31)!=0 ||
       log->io.write(log->io.ctx,off+31,b+31,1)!=TP_OK ||
       log->io.read(log->io.ctx,off+31,&marker,1)!=TP_OK || marker!=COMMIT) {
        log->open=false; return TP_IO_ERROR;
    }
    log->has_records=true; log->latest_slot=slot; log->latest_sequence=seq; e->sequence=seq; return TP_OK;
}
tp_result tp_event_log_read(tp_event_log *log,uint32_t sequence,tp_event *out) {
    if(!log || !log->open || !out) return TP_INVALID;
    for(uint32_t i=0;i<log->slots;i++) {
        uint8_t b[32]; tp_event e;
        if(log->io.read(log->io.ctx,i*32,b,32)!=TP_OK) { log->open=false; return TP_IO_ERROR; }
        if(decode(b,&e) && e.sequence==sequence) { *out=e; return TP_OK; }
    }
    return TP_INVALID;
}
