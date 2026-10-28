#include "tp_upgrade_fram.h"
#include "tp_signed_manifest.h"
#include <string.h>
#define SLOT_SIZE 128u
#define CHUNK 16u
_Static_assert(TP_SIGNED_MANIFEST_SIZE + 4u <= SLOT_SIZE, "manifest slot too small");
static tp_result access(tp_upgrade_fram_region *r,unsigned slot,uint32_t off,void *buf,size_t n,bool write) {
    if(!r || !r->bus.write || !r->bus.write_read || slot>1 || off>SLOT_SIZE || n>SLOT_SIZE-off || (!buf && n)) return TP_INVALID;
    uint32_t addr=r->base+slot*SLOT_SIZE+off; uint8_t *p=buf;
    while(n) {
        size_t len=256u-(addr&255u); if(len>n) len=n; if(len>CHUNK) len=CHUNK;
        uint8_t tx[CHUNK+1]; tx[0]=(uint8_t)addr; uint8_t device=(uint8_t)(0x50u|(addr>>8));
        tp_result result;
        if(write) { memcpy(tx+1,p,len); result=r->bus.write(r->bus.ctx,device,tx,len+1); }
        else result=r->bus.write_read(r->bus.ctx,device,tx,1,p,len);
        if(result!=TP_OK) return TP_IO_ERROR;
        addr+=(uint32_t)len; p+=len; n-=len;
    }
    return TP_OK;
}
static tp_result read_slot(void *ctx,unsigned slot,uint32_t off,void *buf,size_t n) { return access(ctx,slot,off,buf,n,false); }
static tp_result write_slot(void *ctx,unsigned slot,uint32_t off,const void *buf,size_t n) { return access(ctx,slot,off,(void *)buf,n,true); }
static tp_result erase_slot(void *ctx,unsigned slot) {
    tp_upgrade_fram_region *r=ctx; if(!r || slot>1) return TP_INVALID;
    const uint8_t invalid[4]={0,0,0,0}; uint8_t erased[CHUNK]; memset(erased,0xff,sizeof(erased));
    tp_result result=write_slot(r,slot,r->commit,invalid,sizeof(invalid));
    if(result!=TP_OK) return result;
    for(uint32_t off=0;off<SLOT_SIZE;off+=CHUNK) {
        result=write_slot(r,slot,off,erased,sizeof(erased)); if(result!=TP_OK) return result;
    }
    return TP_OK;
}
tp_result tp_upgrade_fram_init(tp_upgrade_fram *f,const tp_i2c_io *bus) {
    if(!f || !bus || !bus->write || !bus->write_read) return TP_INVALID;
    f->boot=(tp_upgrade_fram_region){*bus,0,32};
    f->download=(tp_upgrade_fram_region){*bus,256,64};
    f->manifests=(tp_upgrade_fram_region){*bus,512,TP_SIGNED_MANIFEST_SIZE};
    f->staging=(tp_upgrade_fram_region){*bus,768,64};
    return TP_OK;
}
tp_store_io tp_upgrade_fram_store(tp_upgrade_fram_region *r) {
    tp_store_io io={r,read_slot,erase_slot,write_slot}; return io;
}
