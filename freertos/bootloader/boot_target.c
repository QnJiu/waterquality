#include "tp_boot_target.h"
#include <limits.h>
typedef struct { const tp_boot_target_config *cfg; tp_boot_target target; } context;
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static tp_result verify(void *v,uint8_t bank,uint32_t version) {
    context *c=v; const tp_boot_target_config *cfg=c->cfg; uint32_t size=0;
    tp_result r=cfg->verify(cfg->verify_ctx,bank,version,&size); if(r!=TP_OK) return r;
    if(bank>1 || size<8 || size>cfg->capacity[bank] || size>UINT32_MAX-cfg->base[bank]) return TP_INVALID;
    uint8_t b[8]; r=cfg->read(cfg->read_ctx,bank,0,b,sizeof(b)); if(r!=TP_OK) return r;
    uint32_t sp=le32(b), entry=le32(b+4), addr=entry&~1u; bool stack_ok=false;
    for(unsigned i=0;i<2;i++) if(cfg->ram_start[i]<cfg->ram_end[i] && sp>cfg->ram_start[i] && sp<=cfg->ram_end[i]) stack_ok=true;
    if(!stack_ok || (sp&7u) || !(entry&1u) || addr<cfg->base[bank] || addr-cfg->base[bank]>size-2u) return TP_INVALID;
    c->target=(tp_boot_target){bank,cfg->base[bank],sp,entry}; return TP_OK;
}
tp_result tp_boot_target_prepare(const tp_boot_target_config *cfg,tp_boot_target *out) {
    if(!cfg || !out || !cfg->verify || !cfg->read || !cfg->metadata.read || !cfg->metadata.write || !cfg->metadata.erase ||
       cfg->vector_alignment<128 || (cfg->vector_alignment&(cfg->vector_alignment-1u))) return TP_INVALID;
    for(unsigned i=0;i<2;i++) if((cfg->base[i]&(cfg->vector_alignment-1u)) || cfg->capacity[i]<8 || cfg->capacity[i]>UINT32_MAX-cfg->base[i]) return TP_INVALID;
    uint32_t end0=cfg->base[0]+cfg->capacity[0], end1=cfg->base[1]+cfg->capacity[1];
    if(cfg->base[0]<end1 && cfg->base[1]<end0) return TP_INVALID;
    context c={.cfg=cfg}; tp_boot_io io={cfg->metadata,&c,verify}; uint8_t bank=0;
    tp_result r=tp_boot_select(&io,&bank); if(r!=TP_OK) return r;
    if(c.target.bank!=bank) return TP_INVALID;
    *out=c.target; return TP_OK;
}
