#include "tp_upgrade_wire.h"
#include <string.h>
#define FLAG 0x7eu
#define ESC 0x7du
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffffu;
    while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1) ^ ((c&1u)?0xa001u:0)); }
    return c;
}
static tp_result encode(uint8_t command,uint16_t seq,const uint8_t *data,size_t n,
                        uint8_t *out,size_t cap,size_t *length) {
    if(!out || !length || (n && !data) || n>TP_UP_WIRE_PAYLOAD_MAX) return TP_INVALID;
    uint8_t raw[TP_UP_WIRE_RAW_MAX]; raw[0]=1; raw[1]=command; put16(raw+2,seq); put16(raw+4,(uint16_t)n);
    if(n) memcpy(raw+6,data,n);
    put16(raw+6+n,crc(raw,6+n));
    size_t needed=2;
    for(size_t i=0;i<n+8;i++) needed+=(raw[i]==FLAG || raw[i]==ESC)?2:1;
    if(cap<needed) return TP_INVALID;
    size_t pos=0; out[pos++]=FLAG;
    for(size_t i=0;i<n+8;i++) {
        if(raw[i]==FLAG || raw[i]==ESC) { out[pos++]=ESC; out[pos++]=(uint8_t)(raw[i]^0x20u); }
        else out[pos++]=raw[i];
    }
    out[pos++]=FLAG; *length=pos; return TP_OK;
}
tp_result tp_upgrade_wire_request(uint8_t cmd,uint16_t seq,const uint8_t *data,size_t n,
                                  uint8_t *out,size_t cap,size_t *length) {
    if(cmd<1 || cmd>5) return TP_INVALID;
    return encode(cmd,seq,data,n,out,cap,length);
}
tp_result tp_upgrade_wire_init(tp_upgrade_wire *w,const tp_upgrade_commands *c,void *ctx,
                               tp_result (*send)(void *,const uint8_t *,size_t)) {
    if(!w || !c || !send) return TP_INVALID;
    memset(w,0,sizeof(*w)); w->commands=c; w->send_ctx=ctx; w->send=send; return TP_OK;
}
void tp_upgrade_wire_set_executor(tp_upgrade_wire *w,void *ctx,
    tp_result (*execute)(void *,const tp_upgrade_commands *,tp_upgrade_command,
                         const uint8_t *,size_t,tp_upgrade_reply *)) {
    if(w) { w->execute_ctx=ctx; w->execute=execute; }
}
void tp_upgrade_wire_reset(tp_upgrade_wire *w) {
    if(w) { w->length=0; w->started=false; w->escaped=false; w->discard=false; }
}
static tp_result execute(tp_upgrade_wire *w) {
    const uint8_t *p=w->raw; size_t n=w->length;
    if(n<8 || p[0]!=1 || p[1]<1 || p[1]>5) return TP_INVALID;
    size_t payload=(size_t)p[4] | ((size_t)p[5]<<8);
    if(payload>TP_UP_WIRE_PAYLOAD_MAX || n!=payload+8 ||
       crc(p,n-2)!=((uint16_t)p[n-2] | ((uint16_t)p[n-1]<<8))) return TP_INVALID;
    uint16_t seq=(uint16_t)p[2] | ((uint16_t)p[3]<<8);
    uint8_t cmd=p[1]; tp_upgrade_reply reply={0}; reply.result=TP_INVALID;
    if(w->execute) (void)w->execute(w->execute_ctx,w->commands,(tp_upgrade_command)cmd,p+6,payload,&reply);
    else (void)tp_upgrade_command_execute(w->commands,(tp_upgrade_command)cmd,p+6,payload,&reply);
    uint8_t body[48]={0}; body[0]=(uint8_t)reply.result; body[1]=reply.state_available?1:0;
    size_t body_len=2;
    if(reply.state_available) {
        body[2]=(uint8_t)reply.state.phase; body[3]=reply.state.manifest.bank;
        put32(body+4,reply.state.manifest.version); put32(body+8,reply.state.manifest.size);
        put32(body+12,reply.state.written); memcpy(body+16,reply.state.manifest.id,32); body_len=48;
    }
    uint8_t frame[2*(48+8)+2]; size_t out_len;
    tp_result r=encode((uint8_t)(cmd|0x80u),seq,body,body_len,frame,sizeof(frame),&out_len);
    return r==TP_OK ? w->send(w->send_ctx,frame,out_len) : r;
}
tp_result tp_upgrade_wire_byte(tp_upgrade_wire *w,uint8_t b) {
    if(!w || !w->commands || !w->send) return TP_INVALID;
    if(b==FLAG) {
        tp_result r=TP_OK;
        if(w->started && (w->length || w->escaped || w->discard))
            r=(w->discard || w->escaped) ? TP_INVALID : execute(w);
        w->started=true; w->length=0; w->escaped=false; w->discard=false; return r;
    }
    if(!w->started || w->discard) return TP_OK;
    if(w->escaped) {
        w->escaped=false;
        if(b!=(FLAG^0x20u) && b!=(ESC^0x20u)) { w->discard=true; return TP_INVALID; }
        b^=0x20u;
    } else if(b==ESC) { w->escaped=true; return TP_OK; }
    if(w->length==sizeof(w->raw)) { w->discard=true; return TP_INVALID; }
    w->raw[w->length++]=b; return TP_OK;
}
