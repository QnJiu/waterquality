#include "tp_download.h"
#include <string.h>
#define MAGIC 0x54444c44u
#define FORMAT 1u
#define COMMIT 0x434f4d54u
#define RECORD_SIZE 68u
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t crc32(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(c>>1)^(0xedb88320u & (uint32_t)-(int32_t)(c&1u)); } return ~c;
}
static bool valid(const tp_download_state *s) {
    return s->manifest.bank<2 && s->manifest.version && s->manifest.size &&
        s->phase>=TP_DOWNLOAD_ERASING && s->phase<=TP_DOWNLOAD_CANCELLED &&
        s->written<=s->manifest.size && (s->phase!=TP_DOWNLOAD_ERASING || !s->written) &&
        (s->phase!=TP_DOWNLOAD_VERIFIED || s->written==s->manifest.size);
}
static void encode(uint8_t *p,const tp_download_state *s) {
    memset(p,0,RECORD_SIZE); put32(p,MAGIC); put32(p+4,FORMAT); put32(p+8,s->sequence);
    put32(p+12,s->manifest.version); put32(p+16,s->manifest.size); put32(p+20,s->written);
    memcpy(p+24,s->manifest.id,32); p[56]=s->manifest.bank; p[57]=(uint8_t)s->phase;
    put32(p+60,crc32(p,60)); put32(p+64,COMMIT);
}
static bool decode(const uint8_t *p,tp_download_state *s) {
    if(get32(p)!=MAGIC || get32(p+4)!=FORMAT || get32(p+64)!=COMMIT || get32(p+60)!=crc32(p,60)) return false;
    s->sequence=get32(p+8); s->manifest.version=get32(p+12); s->manifest.size=get32(p+16); s->written=get32(p+20);
    memcpy(s->manifest.id,p+24,32); s->manifest.bank=p[56]; s->phase=(tp_download_phase)p[57]; return valid(s);
}
static bool io_valid(const tp_download_io *io) {
    return io && io->metadata.read && io->metadata.erase && io->metadata.write && io->erase_bank && io->read_bank && io->write_bank && io->verify;
}
static tp_result find(const tp_download_io *io,tp_download_state *s,unsigned *slot) {
    if(!io_valid(io) || !s || !slot) return TP_INVALID;
    bool found=false; uint8_t b[RECORD_SIZE]; tp_download_state tmp;
    for(unsigned i=0;i<2;i++) {
        if(io->metadata.read(io->metadata.ctx,i,0,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
        if(decode(b,&tmp) && (!found || (int32_t)(tmp.sequence-s->sequence)>0)) { *s=tmp; *slot=i; found=true; }
    }
    return found ? TP_OK : TP_INVALID;
}
static tp_result commit(const tp_download_io *io,tp_download_state s,unsigned dest) {
    uint8_t b[RECORD_SIZE],check[RECORD_SIZE]; encode(b,&s); const tp_store_io *m=&io->metadata;
    if(m->erase(m->ctx,dest)!=TP_OK || m->write(m->ctx,dest,0,b,64)!=TP_OK ||
       m->read(m->ctx,dest,0,check,64)!=TP_OK || memcmp(b,check,64)) return TP_IO_ERROR;
    if(m->write(m->ctx,dest,64,b+64,4)!=TP_OK || m->read(m->ctx,dest,0,check,RECORD_SIZE)!=TP_OK || memcmp(b,check,RECORD_SIZE)) return TP_IO_ERROR;
    return TP_OK;
}
tp_result tp_download_read(const tp_download_io *io,tp_download_state *s) { unsigned slot; return find(io,s,&slot); }
tp_result tp_download_begin(const tp_download_io *io,const tp_download_manifest *manifest) {
    if(!io_valid(io) || !manifest || manifest->bank>1 || !manifest->version || !manifest->size) return TP_INVALID;
    tp_download_state s; unsigned slot=1; tp_result r=find(io,&s,&slot);
    if(r==TP_OK && s.phase!=TP_DOWNLOAD_CANCELLED && (s.phase!=TP_DOWNLOAD_VERIFIED ||
       (s.manifest.bank==manifest->bank && s.manifest.version==manifest->version &&
        s.manifest.size==manifest->size && !memcmp(s.manifest.id,manifest->id,TP_DOWNLOAD_ID_SIZE))))
        return TP_BUSY;
    if(r!=TP_OK && r!=TP_INVALID) return r;
    if(r==TP_INVALID) {
        uint8_t b[RECORD_SIZE];
        for(unsigned i=0;i<2;i++) {
            if(io->metadata.read(io->metadata.ctx,i,0,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
            for(unsigned j=0;j<sizeof(b);j++) if(b[j]!=0xffu) return TP_INVALID;
        }
        memset(&s,0,sizeof(s));
    }
    s.manifest=*manifest; s.phase=TP_DOWNLOAD_ERASING; s.written=0; s.sequence++;
    r=commit(io,s,slot^1u); if(r!=TP_OK) return r;
    return tp_download_resume(io);
}
tp_result tp_download_resume(const tp_download_io *io) {
    tp_download_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase==TP_DOWNLOAD_CANCELLED) return TP_INVALID;
    if(s.phase!=TP_DOWNLOAD_ERASING) return TP_OK;
    if(io->erase_bank(io->image_ctx,s.manifest.bank)!=TP_OK) return TP_IO_ERROR;
    s.phase=TP_DOWNLOAD_WRITING; s.sequence++; return commit(io,s,slot^1u);
}
tp_result tp_download_write(const tp_download_io *io,uint32_t offset,const void *data,size_t n) {
    tp_download_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase!=TP_DOWNLOAD_WRITING || !data || !n || n>TP_DOWNLOAD_CHUNK_MAX || offset!=s.written || n>s.manifest.size-offset) return TP_INVALID;
    uint8_t check[TP_DOWNLOAD_CHUNK_MAX];
    if(io->write_bank(io->image_ctx,s.manifest.bank,offset,data,n)!=TP_OK ||
       io->read_bank(io->image_ctx,s.manifest.bank,offset,check,n)!=TP_OK || memcmp(data,check,n)) return TP_IO_ERROR;
    s.written+=(uint32_t)n; s.sequence++; return commit(io,s,slot^1u);
}
tp_result tp_download_cancel(const tp_download_io *io) {
    tp_download_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase==TP_DOWNLOAD_CANCELLED) return TP_OK;
    if(s.phase!=TP_DOWNLOAD_ERASING && s.phase!=TP_DOWNLOAD_WRITING) return TP_INVALID;
    s.phase=TP_DOWNLOAD_CANCELLED; s.sequence++; return commit(io,s,slot^1u);
}
tp_result tp_download_finish(const tp_download_io *io) {
    tp_download_state s; unsigned slot; tp_result r=find(io,&s,&slot); if(r!=TP_OK) return r;
    if(s.phase==TP_DOWNLOAD_VERIFIED) return TP_OK;
    if(s.phase!=TP_DOWNLOAD_WRITING || s.written!=s.manifest.size) return TP_INVALID;
    r=io->verify(io->image_ctx,&s.manifest); if(r!=TP_OK) return r;
    s.phase=TP_DOWNLOAD_VERIFIED; s.sequence++; return commit(io,s,slot^1u);
}
