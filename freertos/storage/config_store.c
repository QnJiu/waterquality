#include "tp_store.h"
#include <math.h>
#include <string.h>
/* Explicit little-endian wire format, independent of structure padding.
 * CRC covers magic through payload; commit word is programmed last. */
_Static_assert(sizeof(float) == 4, "storage format requires 32-bit float");
#define MAGIC 0x54504346u
#define VERSION 2u
#define PAYLOAD_SIZE 58u
#define RECORD_SIZE (16u + PAYLOAD_SIZE + 8u)
#define COMMIT 0x434f4d54u
#define SLOT_SIZE 128u
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void put32(uint8_t *p, uint32_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); p[2]=(uint8_t)(x>>16); p[3]=(uint8_t)(x>>24); }
static float getfloat(const uint8_t *p) { uint32_t u=get32(p); float f; memcpy(&f,&u,4); return f; }
static void putfloat(uint8_t *p, float f) { uint32_t u; memcpy(&u,&f,4); put32(p,u); }
static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t c=0xffffffffu; while(n--) { c ^= *p++; for(unsigned i=0;i<8;i++) c=(c>>1) ^ (0xedb88320u & (uint32_t)-(int32_t)(c&1u)); } return ~c;
}
tp_result tp_config_validate(const tp_config *c) {
    if (!c || !isfinite(c->calibration.slope) || !isfinite(c->calibration.intercept) ||
        !isfinite(c->calibration.turbidity_factor) || !isfinite(c->calibration.min_signal) ||
        !isfinite(c->calibration.max_concentration) || c->calibration.min_signal<=0 ||
        c->calibration.max_concentration<=0 || !isfinite(c->kp) || !isfinite(c->ki) ||
        !isfinite(c->kd) || !isfinite(c->deadband_c) || !isfinite(c->output_max) ||
        !isfinite(c->integral_max) || !isfinite(c->max_temperature_c) || c->kp<0 || c->ki<0 ||
        c->kd<0 || c->deadband_c<0 || c->output_max<=0 || c->output_max>1 ||
        c->integral_max<0 || c->max_temperature_c<=0 || c->period_seconds==0 ||
        c->period_seconds > 0x7fffffffu / 1000u || c->slave_address==0 || c->slave_address>247 ||
        c->mode>2 || c->daily_minute>=1440 || c->utc_offset_minutes < -720 ||
        c->utc_offset_minutes > 840) return TP_INVALID;
    return TP_OK;
}
static void encode(uint8_t *b, const tp_config *c, uint32_t seq) {
    memset(b,0,RECORD_SIZE); put32(b,MAGIC); put32(b+4,VERSION); put32(b+8,seq); put32(b+12,PAYLOAD_SIZE);
    float f[]={c->calibration.slope,c->calibration.intercept,c->calibration.turbidity_factor,
      c->calibration.min_signal,c->calibration.max_concentration,c->kp,c->ki,c->kd,c->deadband_c,
      c->output_max,c->integral_max,c->max_temperature_c};
    for(unsigned i=0;i<12;i++) putfloat(b+16+4*i,f[i]);
    put32(b+64,c->period_seconds); b[68]=c->slave_address;
    b[69]=c->mode; b[70]=(uint8_t)c->daily_minute; b[71]=(uint8_t)(c->daily_minute>>8);
    uint16_t offset=(uint16_t)c->utc_offset_minutes; b[72]=(uint8_t)offset; b[73]=(uint8_t)(offset>>8);
    put32(b+74,crc32(b,74)); put32(b+78,COMMIT);
}
static bool decode(const uint8_t *b, tp_config *c, uint32_t *seq) {
    uint32_t version=get32(b+4), payload=get32(b+12);
    if(get32(b)!=MAGIC || !((version==1u && payload==53u) ||
       (version==VERSION && payload==PAYLOAD_SIZE))) return false;
    size_t crc_offset=16u+payload;
    if(get32(b+crc_offset+4)!=COMMIT || get32(b+crc_offset)!=crc32(b,crc_offset)) return false;
    c->calibration.slope=getfloat(b+16); c->calibration.intercept=getfloat(b+20);
    c->calibration.turbidity_factor=getfloat(b+24); c->calibration.min_signal=getfloat(b+28);
    c->calibration.max_concentration=getfloat(b+32); c->kp=getfloat(b+36); c->ki=getfloat(b+40);
    c->kd=getfloat(b+44); c->deadband_c=getfloat(b+48); c->output_max=getfloat(b+52);
    c->integral_max=getfloat(b+56); c->max_temperature_c=getfloat(b+60);
    c->period_seconds=get32(b+64); c->slave_address=b[68]; *seq=get32(b+8);
    c->mode=0; c->daily_minute=0; c->utc_offset_minutes=0;
    if(version==VERSION) {
        c->mode=b[69]; c->daily_minute=(uint16_t)(b[70] | ((uint16_t)b[71]<<8));
        uint16_t raw=(uint16_t)(b[72] | ((uint16_t)b[73]<<8));
        c->utc_offset_minutes=(int16_t)(raw<=32767u ? (int32_t)raw : (int32_t)raw-65536);
    }
    return tp_config_validate(c)==TP_OK;
}
/* Versions share the 16-byte header and trailing CRC/commit envelope.
 * A valid unfamiliar record is preserved, even alongside a known record. */
static bool unsupported_record(const uint8_t *b) {
    uint32_t size=get32(b+12);
    if (get32(b)!=MAGIC || size>SLOT_SIZE-24u) return false;
    if ((get32(b+4)==VERSION && size==PAYLOAD_SIZE) || (get32(b+4)==1u && size==53u)) return false;
    size_t crc_offset=16u+size;
    return get32(b+crc_offset+4)==COMMIT && get32(b+crc_offset)==crc32(b,crc_offset);
}
static tp_result find(const tp_store_io *io, tp_config *cfg, uint32_t *seq, unsigned *slot, bool *nonblank) {
    uint8_t b[SLOT_SIZE]; tp_config temp, best; uint32_t s, best_seq=0;
    unsigned best_slot=0; bool found=false, unsupported=false, has_data=false;
    if(!io || !io->read || !cfg || !seq || !slot) return TP_INVALID;
    for(unsigned i=0;i<2;i++) {
        if(io->read(io->ctx,i,0,b,sizeof(b))!=TP_OK) return TP_IO_ERROR;
        for (size_t j=0; j<sizeof(b); j++) if (b[j]!=0xffu) { has_data=true; break; }
        if(unsupported_record(b)) unsupported=true;
        else if(decode(b,&temp,&s) && (!found || (int32_t)(s-best_seq)>0)) {
            best=temp; best_seq=s; best_slot=i; found=true;
        }
    }
    if(nonblank) *nonblank=has_data;
    if(unsupported) return TP_UNSUPPORTED;
    if(!found) return TP_INVALID;
    *cfg=best; *seq=best_seq; *slot=best_slot; return TP_OK;
}
tp_result tp_store_load(const tp_store_io *io, tp_config *cfg, uint32_t *sequence) {
    unsigned slot; return find(io,cfg,sequence,&slot,NULL);
}
tp_result tp_store_save(const tp_store_io *io, const tp_config *cfg) {
    if(!io || !io->read || !io->erase || !io->write || tp_config_validate(cfg)!=TP_OK) return TP_INVALID;
    tp_config old; uint32_t seq=0; unsigned slot=1; tp_result r=find(io,&old,&seq,&slot,NULL);
    if(r==TP_IO_ERROR) return r;
    if(r!=TP_OK && r!=TP_INVALID) return r;
    unsigned dest=slot^1u; uint8_t b[RECORD_SIZE], check[RECORD_SIZE]; encode(b,cfg,seq+1u);
    if(io->erase(io->ctx,dest)!=TP_OK || io->write(io->ctx,dest,0,b,RECORD_SIZE-4)!=TP_OK ||
       io->read(io->ctx,dest,0,check,RECORD_SIZE-4)!=TP_OK || memcmp(b,check,RECORD_SIZE-4)) return TP_IO_ERROR;
    if(io->write(io->ctx,dest,RECORD_SIZE-4,b+RECORD_SIZE-4,4)!=TP_OK ||
       io->read(io->ctx,dest,0,check,RECORD_SIZE)!=TP_OK || memcmp(b,check,RECORD_SIZE)) return TP_IO_ERROR;
    return TP_OK;
}



tp_result tp_store_bootstrap(const tp_store_io *io, const tp_config *defaults,
    tp_config *out, uint32_t *sequence, tp_config_source *source) {
    if (!out || !sequence || !source || tp_config_validate(defaults) != TP_OK)
        return TP_INVALID;
    tp_config loaded; uint32_t seq; unsigned slot; bool nonblank=false;
    tp_result r = find(io, &loaded, &seq, &slot, &nonblank);
    if (r == TP_OK) {
        *out = loaded; *sequence = seq; *source = TP_CONFIG_FROM_STORAGE;
        return TP_OK;
    }
    /* TP_INVALID means there is no valid record only when the interface itself
     * is valid. Validate it separately before falling back to defaults. */
    if (!io || !io->read) return TP_INVALID;
    if (r != TP_INVALID) return r;
    *out = *defaults; *sequence = 0; *source = nonblank ? TP_CONFIG_CORRUPT_DEFAULTED : TP_CONFIG_DEFAULTED;
    return TP_OK;
}
