#include "tp_w25q128_image.h"
#include <string.h>
static bool valid(const tp_w25q128_image *i) {
    return i && i->flash && i->now_ms && i->sleep_ms && i->size && i->protected_bank<2;
}
static bool bounds(const tp_w25q128_image *i,uint8_t bank,uint32_t offset,size_t n) {
    return valid(i) && bank<2 && offset<=i->size && n<=i->size-offset;
}
tp_result tp_w25q128_image_init(tp_w25q128_image *i,tp_w25q128 *f,void *ctx,
    uint32_t (*now)(void *),void (*sleep)(void *),uint32_t a,uint32_t b,uint32_t size,
    uint8_t protect,uint32_t program_timeout,uint32_t erase_timeout) {
    if (!i || !f || !f->ready || f->pending || !now || !sleep || protect>1 || !size ||
        a%TP_W25Q128_SECTOR_SIZE || b%TP_W25Q128_SECTOR_SIZE || size%TP_W25Q128_SECTOR_SIZE ||
        a>=TP_W25Q128_SIZE || b>=TP_W25Q128_SIZE || size>TP_W25Q128_SIZE-a ||
        size>TP_W25Q128_SIZE-b || (a<b ? b-a<size : a-b<size) ||
        !program_timeout || program_timeout>0x7fffffffu || !erase_timeout || erase_timeout>0x7fffffffu)
        return TP_INVALID;
    *i=(tp_w25q128_image){f,ctx,now,sleep,{a,b},size,program_timeout,erase_timeout,protect};
    return TP_OK;
}
tp_result tp_w25q128_image_set_protected(tp_w25q128_image *i,uint8_t bank) {
    if (!valid(i) || bank>1 || !i->flash->ready || i->flash->pending) return TP_INVALID;
    i->protected_bank=bank; return TP_OK;
}
static tp_result wait_ready(tp_w25q128_image *i) {
    for (;;) {
        tp_result r=tp_w25q128_poll(i->flash,i->now_ms(i->clock_ctx));
        if(r!=TP_BUSY) return r;
        i->sleep_ms(i->clock_ctx);
    }
}
tp_result tp_w25q128_image_read(void *ctx,uint8_t bank,uint32_t offset,void *out,size_t n) {
    tp_w25q128_image *i=ctx;
    if (!bounds(i,bank,offset,n) || (!out && n)) return TP_INVALID;
    uint8_t *dest=out;
    while(n) {
        size_t count=n<TP_W25Q128_PAGE_SIZE?n:TP_W25Q128_PAGE_SIZE;
        tp_result r=tp_w25q128_read(i->flash,i->base[bank]+offset,dest,count);
        if(r!=TP_OK) return r;
        offset+=(uint32_t)count; dest+=count; n-=count;
    }
    return TP_OK;
}
tp_result tp_w25q128_image_write(void *ctx,uint8_t bank,uint32_t offset,const void *data,size_t n) {
    tp_w25q128_image *i=ctx;
    if (!bounds(i,bank,offset,n) || bank==i->protected_bank || !data || !n || n>256) return TP_INVALID;
    const uint8_t *src=data; uint8_t check[256];
    tp_result r=tp_w25q128_image_read(i,bank,offset,check,n); if(r!=TP_OK) return r;
    for(size_t j=0;j<n;j++) if((check[j]&src[j])!=src[j]) return TP_INVALID;
    size_t done=0;
    while(done<n) {
        uint32_t address=i->base[bank]+offset+(uint32_t)done;
        size_t count=TP_W25Q128_PAGE_SIZE-(address%TP_W25Q128_PAGE_SIZE);
        if(count>n-done) count=n-done;
        if(memcmp(check+done,src+done,count)) {
            r=tp_w25q128_program(i->flash,address,src+done,count,i->now_ms(i->clock_ctx),i->program_timeout_ms);
            if(r!=TP_OK) return r;
            r=wait_ready(i); if(r!=TP_OK) return r;
        }
        done+=count;
    }
    r=tp_w25q128_image_read(i,bank,offset,check,n);
    return r!=TP_OK ? r : memcmp(check,src,n) ? TP_IO_ERROR : TP_OK;
}
tp_result tp_w25q128_image_erase(void *ctx,uint8_t bank) {
    tp_w25q128_image *i=ctx;
    if(!valid(i) || bank>1 || bank==i->protected_bank) return TP_INVALID;
    uint8_t check[256];
    for(uint32_t offset=0;offset<i->size;offset+=TP_W25Q128_SECTOR_SIZE) {
        tp_result r=tp_w25q128_erase(i->flash,i->base[bank]+offset,i->now_ms(i->clock_ctx),i->erase_timeout_ms);
        if(r!=TP_OK) return r;
        r=wait_ready(i); if(r!=TP_OK) return r;
        for(uint32_t pos=0;pos<TP_W25Q128_SECTOR_SIZE;pos+=sizeof(check)) {
            r=tp_w25q128_image_read(i,bank,offset+pos,check,sizeof(check)); if(r!=TP_OK) return r;
            for(size_t j=0;j<sizeof(check);j++) if(check[j]!=0xff) return TP_IO_ERROR;
        }
    }
    return TP_OK;
}
