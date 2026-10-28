#include "tp_f407_image_flash.h"

static bool bounds(uint8_t bank,uint32_t offset,size_t n) {
    return bank<2 && offset<=TP_F407_IMAGE_BANK_SIZE && n<=TP_F407_IMAGE_BANK_SIZE-offset;
}
static uint32_t base(uint8_t bank) { return bank ? TP_F407_IMAGE1_BASE : TP_F407_IMAGE0_BASE; }
static bool valid(const tp_f407_image_flash *f) {
    return f && f->active_bank<2 && f->ops.read && f->ops.erase_sector && f->ops.program_byte;
}
tp_result tp_f407_image_flash_init(tp_f407_image_flash *f,const tp_f407_flash_ops *ops,uint8_t active) {
    if(!f || !ops || !ops->read || !ops->erase_sector || !ops->program_byte || active>1) return TP_INVALID;
    tp_f407_flash_ops saved=*ops; f->ops=saved; f->active_bank=active; return TP_OK;
}
tp_result tp_f407_image_flash_set_active(tp_f407_image_flash *f,uint8_t active) {
    if(!valid(f) || active>1) return TP_INVALID;
    f->active_bank=active; return TP_OK;
}
tp_result tp_f407_image_read(void *ctx,uint8_t bank,uint32_t offset,void *out,size_t n) {
    tp_f407_image_flash *f=ctx;
    if(!valid(f) || !bounds(bank,offset,n) || (!out && n)) return TP_INVALID;
    return n ? f->ops.read(f->ops.ctx,base(bank)+offset,out,n) : TP_OK;
}
tp_result tp_f407_image_erase(void *ctx,uint8_t bank) {
    tp_f407_image_flash *f=ctx;
    if(!valid(f) || bank>1 || bank==f->active_bank) return TP_INVALID;
    unsigned first=bank ? 8u : 5u;
    for(unsigned sector=first;sector<first+3u;sector++) {
        tp_result r=f->ops.erase_sector(f->ops.ctx,sector); if(r!=TP_OK) return r;
    }
    return TP_OK;
}
tp_result tp_f407_image_write(void *ctx,uint8_t bank,uint32_t offset,const void *data,size_t n) {
    tp_f407_image_flash *f=ctx;
    if(!valid(f) || !data || !n || !bounds(bank,offset,n) || bank==f->active_bank) return TP_INVALID;
    const uint8_t *src=data; uint8_t old[64]; uint32_t address=base(bank)+offset;
    /* Preflight the full request so incompatible bit transitions do not cause
     * a partial write; a repeated identical write is safe after interruption. */
    for(size_t pos=0;pos<n;) {
        size_t count=n-pos<sizeof(old) ? n-pos : sizeof(old);
        tp_result r=f->ops.read(f->ops.ctx,address+(uint32_t)pos,old,count); if(r!=TP_OK) return r;
        for(size_t i=0;i<count;i++) if((old[i]&src[pos+i])!=src[pos+i]) return TP_INVALID;
        pos+=count;
    }
    for(size_t pos=0;pos<n;pos++) {
        uint8_t current;
        tp_result r=f->ops.read(f->ops.ctx,address+(uint32_t)pos,&current,1); if(r!=TP_OK) return r;
        if(current!=src[pos]) {
            r=f->ops.program_byte(f->ops.ctx,address+(uint32_t)pos,src[pos]); if(r!=TP_OK) return r;
            r=f->ops.read(f->ops.ctx,address+(uint32_t)pos,&current,1); if(r!=TP_OK) return r;
            if(current!=src[pos]) return TP_IO_ERROR;
        }
    }
    return TP_OK;
}
