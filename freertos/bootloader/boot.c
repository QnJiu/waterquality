#include "tp_boot.h"
#include <string.h>
#define MAGIC 0x54424f54u
#define FORMAT 1u
#define COMMIT 0x434f4d54u
#define RECORD_SIZE 36u
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t crc32(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(c>>1)^(0xedb88320u & (uint32_t)-(int32_t)(c&1u)); } return ~c;
}
static bool valid(const tp_boot_state *s) {
    if(s->active_bank>1 || !s->active_version || s->phase>TP_BOOT_TRIAL) return false;
    if(s->phase==TP_BOOT_IDLE) return !s->candidate_version && s->candidate_bank==0;
    return s->candidate_bank<=1 && s->candidate_bank!=s->active_bank && s->candidate_version>s->active_version;
}
static void encode(uint8_t *p,const tp_boot_state *s) {
    memset(p,0,RECORD_SIZE); put32(p,MAGIC); put32(p+4,FORMAT); put32(p+8,s->sequence);
    put32(p+12,s->active_version); put32(p+16,s->candidate_version);
    p[20]=s->active_bank; p[21]=s->candidate_bank; p[22]=(uint8_t)s->phase;
    put32(p+28,crc32(p,28)); put32(p+32,COMMIT);
}
static bool decode(const uint8_t *p,tp_boot_state *s) {
    if(get32(p)!=MAGIC || get32(p+4)!=FORMAT || get32(p+32)!=COMMIT || get32(p+28)!=crc32(p,28)) return false;
    s->sequence=get32(p+8); s->active_version=get32(p+12); s->candidate_version=get32(p+16);
    s->active_bank=p[20]; s->candidate_bank=p[21]; s->phase=(tp_boot_phase)p[22]; return valid(s);
}
static bool io_valid(const tp_boot_io *io) {
    return io && io->metadata.read && io->metadata.erase && io->metadata.write && io->verify_image;
}
static tp_result find(const tp_boot_io *io,tp_boot_state *s,unsigned *slot) {
    if(!io_valid(io) || !s || !slot) return TP_INVALID;
    uint8_t b[RECORD_SIZE]; tp_boot_state tmp; bool found=false;
    for(unsigned i=0;i<2;i++) {
        if(io->metadata.read(io->metadata.ctx,i,0,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
        if(decode(b,&tmp) && (!found || (int32_t)(tmp.sequence-s->sequence)>0)) { *s=tmp; *slot=i; found=true; }
    }
    return found ? TP_OK : TP_INVALID;
}
static tp_result commit(const tp_boot_io *io,tp_boot_state s,unsigned dest) {
    uint8_t b[RECORD_SIZE],check[RECORD_SIZE]; encode(b,&s);
    const tp_store_io *m=&io->metadata;
    if(m->erase(m->ctx,dest)!=TP_OK || m->write(m->ctx,dest,0,b,32)!=TP_OK ||
       m->read(m->ctx,dest,0,check,32)!=TP_OK || memcmp(b,check,32)) return TP_IO_ERROR;
    if(m->write(m->ctx,dest,32,b+32,4)!=TP_OK ||
       m->read(m->ctx,dest,0,check,RECORD_SIZE)!=TP_OK || memcmp(b,check,RECORD_SIZE)) return TP_IO_ERROR;
    return TP_OK;
}
static void clear_candidate(tp_boot_state *s) { s->phase=TP_BOOT_IDLE; s->candidate_bank=0; s->candidate_version=0; }
tp_result tp_boot_read(const tp_boot_io *io,tp_boot_state *s) { unsigned slot; return find(io,s,&slot); }
tp_result tp_boot_metadata_blank(const tp_boot_io *io) {
    if(!io_valid(io)) return TP_INVALID;
    uint8_t b[RECORD_SIZE];
    for(unsigned i=0;i<2;i++) {
        if(io->metadata.read(io->metadata.ctx,i,0,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
        for(unsigned j=0;j<sizeof(b);j++) if(b[j]!=0xffu) return TP_INVALID;
    }
    return TP_OK;
}
tp_result tp_boot_provision(const tp_boot_io *io,uint8_t bank,uint32_t version) {
    if(!io_valid(io) || bank>1 || !version) return TP_INVALID;
    tp_boot_state s; unsigned slot; tp_result r=find(io,&s,&slot);
    if(r==TP_OK) return TP_BUSY;
    if(r!=TP_INVALID) return r;
    /* Invalid or damaged metadata is not automatically factory blank. */
    r=tp_boot_metadata_blank(io); if(r!=TP_OK) return r;
    r=io->verify_image(io->image_ctx,bank,version); if(r!=TP_OK) return r;
    memset(&s,0,sizeof(s)); s.sequence=1; s.active_bank=bank; s.active_version=version;
    return commit(io,s,0);
}
tp_result tp_boot_stage(const tp_boot_io *io,uint8_t bank,uint32_t version) {
    tp_boot_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase!=TP_BOOT_IDLE) return TP_BUSY;
    if(bank>1 || bank==s.active_bank || version<=s.active_version) return TP_INVALID;
    r=io->verify_image(io->image_ctx,bank,version); if(r!=TP_OK) return r;
    s.candidate_bank=bank; s.candidate_version=version; s.phase=TP_BOOT_READY; s.sequence++;
    return commit(io,s,slot^1u);
}
tp_result tp_boot_select(const tp_boot_io *io,uint8_t *bank) {
    if(!bank) return TP_INVALID;
    tp_boot_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase==TP_BOOT_READY) {
        r=io->verify_image(io->image_ctx,s.candidate_bank,s.candidate_version);
        if(r==TP_OK) {
            uint8_t candidate=s.candidate_bank; s.phase=TP_BOOT_TRIAL; s.sequence++;
            r=commit(io,s,slot^1u); if(r==TP_OK) *bank=candidate; return r;
        }
        if(r!=TP_INVALID) return r;
    }
    r=io->verify_image(io->image_ctx,s.active_bank,s.active_version); if(r!=TP_OK) return r;
    if(s.phase!=TP_BOOT_IDLE) { clear_candidate(&s); s.sequence++; r=commit(io,s,slot^1u); if(r!=TP_OK) return r; }
    *bank=s.active_bank; return TP_OK;
}
tp_result tp_boot_confirm(const tp_boot_io *io,uint8_t bank) {
    tp_boot_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase!=TP_BOOT_TRIAL || bank!=s.candidate_bank) return TP_INVALID;
    r=io->verify_image(io->image_ctx,bank,s.candidate_version); if(r!=TP_OK) return r;
    s.active_bank=bank; s.active_version=s.candidate_version; clear_candidate(&s); s.sequence++;
    return commit(io,s,slot^1u);
}
