#include "tp_mb85rc16.h"
#include <string.h>
#define MEMORY_SIZE 2048u
#define SLOT_SIZE 128u
#define WRITE_CHUNK 16u
#define COMMIT_OFFSET 73u
static tp_result access(tp_mb85rc16 *d, unsigned slot, uint32_t off, void *buffer,
                        size_t n, bool write) {
    if (!d || slot > 1 || off > SLOT_SIZE || n > SLOT_SIZE - off || (!buffer && n)) return TP_INVALID;
    uint32_t addr = d->base + slot * SLOT_SIZE + off;
    uint8_t *p = buffer;
    while (n) {
        size_t chunk = 256u - (addr & 255u);
        if (chunk > n) chunk = n;
        if (chunk > WRITE_CHUNK) chunk = WRITE_CHUNK;
        uint8_t device = (uint8_t)(0x50u | (addr >> 8));
        uint8_t tx[WRITE_CHUNK + 1]; tx[0] = (uint8_t)addr;
        tp_result r;
        if (write) { memcpy(tx + 1, p, chunk); r = d->io.write(d->io.ctx, device, tx, chunk + 1); }
        else r = d->io.write_read(d->io.ctx, device, tx, 1, p, chunk);
        if (r != TP_OK) return TP_IO_ERROR;
        addr += (uint32_t)chunk; p += chunk; n -= chunk;
    }
    return TP_OK;
}
static tp_result read_slot(void *ctx, unsigned slot, uint32_t off, void *buf, size_t n) {
    return access(ctx,slot,off,buf,n,false);
}
static tp_result write_slot(void *ctx, unsigned slot, uint32_t off, const void *buf, size_t n) {
    return access(ctx,slot,off,(void *)buf,n,true);
}
static tp_result erase_slot(void *ctx, unsigned slot) {
    /* Invalidate commit first, before modifying any other record bytes. */
    uint8_t invalid[4] = {0,0,0,0};
    tp_result r = write_slot(ctx,slot,COMMIT_OFFSET,invalid,sizeof(invalid));
    if (r != TP_OK) return r;
    uint8_t erased[WRITE_CHUNK]; memset(erased,0xff,sizeof(erased));
    for (uint32_t off=0; off<SLOT_SIZE; off+=sizeof(erased)) {
        r = write_slot(ctx,slot,off,erased,sizeof(erased));
        if (r != TP_OK) return r;
    }
    return TP_OK;
}
tp_result tp_mb85rc16_init(tp_mb85rc16 *d, const tp_i2c_io *io, uint16_t base) {
    if (!d || !io || !io->write || !io->write_read || base % 256u ||
        (uint32_t)base + SLOT_SIZE * 2u > MEMORY_SIZE) return TP_INVALID;
    d->io = *io; d->base = base; return TP_OK;
}
tp_store_io tp_mb85rc16_store(tp_mb85rc16 *d) {
    tp_store_io io = {d, read_slot, erase_slot, write_slot}; return io;
}

static tp_result log_access(tp_mb85rc16_log_region *region,uint32_t offset,
                            void *buffer,size_t size,bool write) {
    if(!region || !region->dev || offset>region->size || size>region->size-offset || (!buffer && size)) return TP_INVALID;
    tp_mb85rc16 *dev=region->dev; uint32_t address=region->start+offset; uint8_t *p=buffer;
    while(size) {
        size_t n=256u-(address&255u); if(n>size) n=size; if(n>WRITE_CHUNK) n=WRITE_CHUNK;
        uint8_t tx[WRITE_CHUNK+1]; tx[0]=(uint8_t)address; uint8_t unit=(uint8_t)(0x50u|(address>>8));
        tp_result r;
        if(write) { memcpy(tx+1,p,n); r=dev->io.write(dev->io.ctx,unit,tx,n+1); }
        else r=dev->io.write_read(dev->io.ctx,unit,tx,1,p,n);
        if(r!=TP_OK) return TP_IO_ERROR;
        p+=n; address+=(uint32_t)n; size-=n;
    }
    return TP_OK;
}
static tp_result log_read(void *ctx,uint32_t off,void *buf,size_t n) { return log_access(ctx,off,buf,n,false); }
static tp_result log_write(void *ctx,uint32_t off,const void *buf,size_t n) { return log_access(ctx,off,(void *)buf,n,true); }
tp_result tp_mb85rc16_log_init(tp_mb85rc16_log_region *region,tp_mb85rc16 *dev,uint16_t start,uint16_t size) {
    if(!region || !dev || !dev->io.write || !dev->io.write_read || start%32 || size<64 || size%32 ||
       (uint32_t)start+size>MEMORY_SIZE ||
       ((uint32_t)start<(uint32_t)dev->base+2u*SLOT_SIZE && (uint32_t)start+size>dev->base)) return TP_INVALID;
    region->dev=dev; region->start=start; region->size=size; return TP_OK;
}
tp_event_log_io tp_mb85rc16_log_io(tp_mb85rc16_log_region *region) {
    tp_event_log_io io={region,log_read,log_write,region ? region->size : 0}; return io;
}
