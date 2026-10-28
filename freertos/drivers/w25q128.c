#include "tp_w25q128.h"
#include <string.h>

static tp_result transfer(tp_w25q128 *f, const uint8_t *tx, uint8_t *rx, size_t n) {
    if (f->io.transfer(f->io.ctx,tx,rx,n)!=TP_OK) { f->ready=false; return TP_IO_ERROR; }
    return TP_OK;
}
static tp_result status(tp_w25q128 *f, uint8_t *value) {
    uint8_t tx[2]={0x05,0xff}, rx[2];
    if (transfer(f,tx,rx,2)!=TP_OK) return TP_IO_ERROR;
    *value=rx[1]; return TP_OK;
}
static bool bounds(uint32_t a,size_t n) {
    return n && n<=TP_W25Q128_PAGE_SIZE && a<TP_W25Q128_SIZE && n<=TP_W25Q128_SIZE-a;
}
static void header(uint8_t *p,uint8_t command,uint32_t a) {
    p[0]=command; p[1]=(uint8_t)(a>>16); p[2]=(uint8_t)(a>>8); p[3]=(uint8_t)a;
}
tp_result tp_w25q128_init(tp_w25q128 *f,const tp_w25q128_io *io) {
    if (!f || !io || !io->transfer) return TP_INVALID;
    tp_w25q128_io copy=*io; memset(f,0,sizeof(*f)); f->io=copy;
    uint8_t tx[4]={0x9f,0xff,0xff,0xff}, rx[4], sr;
    if (transfer(f,tx,rx,4)!=TP_OK) return TP_IO_ERROR;
    if (rx[1]!=0xef || rx[2]!=0x40 || rx[3]!=0x18) return TP_UNSUPPORTED;
    if (status(f,&sr)!=TP_OK) return TP_IO_ERROR;
    if (sr&1u) return TP_BUSY;
    f->ready=true; return TP_OK;
}
tp_result tp_w25q128_read(tp_w25q128 *f,uint32_t a,void *out,size_t n) {
    if (!f || !f->ready || !out || !bounds(a,n)) return TP_INVALID;
    if (f->pending) return TP_BUSY;
    uint8_t tx[4+TP_W25Q128_PAGE_SIZE],rx[4+TP_W25Q128_PAGE_SIZE];
    header(tx,0x03,a); memset(tx+4,0xff,n);
    if (transfer(f,tx,rx,n+4)!=TP_OK) return TP_IO_ERROR;
    memcpy(out,rx+4,n); return TP_OK;
}
static tp_result start(tp_w25q128 *f,const uint8_t *tx,size_t n,uint32_t now,uint32_t timeout) {
    if (!f || !f->ready || !timeout || timeout>0x7fffffffu) return TP_INVALID;
    if (f->pending) return TP_BUSY;
    uint8_t sr, we=0x06, rx[4+TP_W25Q128_PAGE_SIZE];
    if (status(f,&sr)!=TP_OK) return TP_IO_ERROR;
    if (sr&1u) { f->ready=false; return TP_BUSY; }
    if (transfer(f,&we,rx,1)!=TP_OK || status(f,&sr)!=TP_OK) return TP_IO_ERROR;
    if (!(sr&2u)) { f->ready=false; return TP_IO_ERROR; }
    if (transfer(f,tx,rx,n)!=TP_OK) return TP_IO_ERROR;
    f->pending=true; f->started_ms=now; f->timeout_ms=timeout; return TP_OK;
}
tp_result tp_w25q128_program(tp_w25q128 *f,uint32_t a,const void *data,size_t n,
                              uint32_t now,uint32_t timeout) {
    if (!data || !bounds(a,n) || n>TP_W25Q128_PAGE_SIZE-(a%TP_W25Q128_PAGE_SIZE)) return TP_INVALID;
    uint8_t tx[4+TP_W25Q128_PAGE_SIZE]; header(tx,0x02,a); memcpy(tx+4,data,n);
    return start(f,tx,n+4,now,timeout);
}
tp_result tp_w25q128_erase(tp_w25q128 *f,uint32_t a,uint32_t now,uint32_t timeout) {
    if (a>=TP_W25Q128_SIZE || a%TP_W25Q128_SECTOR_SIZE) return TP_INVALID;
    uint8_t tx[4]; header(tx,0x20,a); return start(f,tx,sizeof(tx),now,timeout);
}
tp_result tp_w25q128_poll(tp_w25q128 *f,uint32_t now) {
    if (!f || !f->ready || !f->pending) return TP_INVALID;
    if ((uint32_t)(now-f->started_ms)>=f->timeout_ms) { f->ready=false; f->pending=false; return TP_TIMEOUT; }
    uint8_t sr; if (status(f,&sr)!=TP_OK) { f->pending=false; return TP_IO_ERROR; }
    if (sr&1u) return TP_BUSY;
    f->pending=false; return TP_OK;
}
