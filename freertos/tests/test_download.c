#include "tp_download.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t slots[2][128], banks[2][512], expected[512];
    int op, fail;
    bool verify_error;
} fake;
static bool failing(fake *f) { return ++f->op == f->fail; }
static tp_result read_slot(void *ctx, unsigned slot, uint32_t off, void *out, size_t n) {
    fake *f=ctx; assert(slot<2 && off+n<=128);
    if(failing(f)) return TP_IO_ERROR;
    memcpy(out,f->slots[slot]+off,n); return TP_OK;
}
static tp_result erase_slot(void *ctx, unsigned slot) {
    fake *f=ctx; assert(slot<2);
    if(failing(f)) return TP_IO_ERROR;
    memset(f->slots[slot],0xff,128); return TP_OK;
}
static tp_result program(uint8_t *dest, const void *data, size_t n) {
    const uint8_t *p=data;
    for(size_t i=0;i<n;i++) {
        if((dest[i]&p[i])!=p[i]) return TP_IO_ERROR;
        dest[i]&=p[i];
    }
    return TP_OK;
}
static tp_result write_slot(void *ctx, unsigned slot, uint32_t off, const void *in, size_t n) {
    fake *f=ctx; assert(slot<2 && off+n<=128);
    if(failing(f)) { assert(program(f->slots[slot]+off,in,n)==TP_OK); return TP_IO_ERROR; }
    return program(f->slots[slot]+off,in,n);
}
static tp_result erase_bank(void *ctx, uint8_t bank) {
    fake *f=ctx; assert(bank<2);
    if(failing(f)) { memset(f->banks[bank],0xff,256); return TP_IO_ERROR; }
    memset(f->banks[bank],0xff,512); return TP_OK;
}
static tp_result read_bank(void *ctx, uint8_t bank, uint32_t off, void *out, size_t n) {
    fake *f=ctx; assert(bank<2 && off+n<=512);
    if(failing(f)) return TP_IO_ERROR;
    memcpy(out,f->banks[bank]+off,n); return TP_OK;
}
static tp_result write_bank(void *ctx, uint8_t bank, uint32_t off, const void *in, size_t n) {
    fake *f=ctx; assert(bank<2 && off+n<=512);
    if(failing(f)) { assert(program(f->banks[bank]+off,in,n/2)==TP_OK); return TP_IO_ERROR; }
    return program(f->banks[bank]+off,in,n);
}
static tp_result verify(void *ctx, const tp_download_manifest *m) {
    fake *f=ctx;
    if(f->verify_error) return TP_IO_ERROR;
    return m->bank==1 && m->version==2 && m->size==512 && m->id[0]==42 &&
        !memcmp(f->banks[1],f->expected,512) ? TP_OK : TP_INVALID;
}
static tp_download_io setup(fake *f) {
    memset(f,0,sizeof(*f)); memset(f->slots,0xff,sizeof(f->slots));
    memset(f->banks,0x55,sizeof(f->banks));
    for(size_t i=0;i<512;i++) f->expected[i]=(uint8_t)(i*17u);
    tp_download_io io={{f,read_slot,erase_slot,write_slot},f,erase_bank,read_bank,write_bank,verify};
    return io;
}
static tp_download_manifest manifest(void) {
    tp_download_manifest m={{42},2,512,1}; return m;
}
static void complete(fake *f, tp_download_io *io) {
    tp_download_state s;
    assert(tp_download_read(io,&s)==TP_OK);
    assert(tp_download_resume(io)==TP_OK);
    assert(tp_download_read(io,&s)==TP_OK && s.phase==TP_DOWNLOAD_WRITING);
    while(s.written<512) {
        assert(tp_download_write(io,s.written,f->expected+s.written,256)==TP_OK);
        assert(tp_download_read(io,&s)==TP_OK);
    }
    assert(tp_download_finish(io)==TP_OK);
    assert(tp_download_read(io,&s)==TP_OK && s.phase==TP_DOWNLOAD_VERIFIED);
    for(size_t i=0;i<512;i++) assert(f->banks[0][i]==0x55);
}
int main(void) {
    fake f; tp_download_io io=setup(&f); tp_download_manifest m=manifest(); tp_download_state s;
    assert(tp_download_begin(&io,&m)==TP_OK);
    assert(tp_download_begin(&io,&m)==TP_BUSY);
    assert(tp_download_finish(&io)==TP_INVALID);
    assert(tp_download_write(&io,1,f.expected,256)==TP_INVALID);
    assert(tp_download_write(&io,0,f.expected,257)==TP_INVALID);
    complete(&f,&io);
    io=setup(&f); f.slots[0][0]=0; assert(tp_download_begin(&io,&m)==TP_INVALID);
    for(int at=1;at<=8;at++) {
        io=setup(&f); f.op=0; f.fail=at;
        assert(tp_download_begin(&io,&m)==TP_IO_ERROR);
        f.fail=0;
        tp_result r=tp_download_read(&io,&s);
        if(r==TP_OK) complete(&f,&io);
        else assert(r==TP_INVALID);
    }
    for(int at=1;at<=9;at++) {
        io=setup(&f); assert(tp_download_begin(&io,&m)==TP_OK);
        f.op=0; f.fail=at; assert(tp_download_write(&io,0,f.expected,256)==TP_IO_ERROR);
        f.fail=0; assert(tp_download_read(&io,&s)==TP_OK);
        assert(s.written==0 || s.written==256);
        complete(&f,&io);
    }
    io=setup(&f); assert(tp_download_begin(&io,&m)==TP_OK);
    assert(tp_download_write(&io,0,f.expected,256)==TP_OK);
    assert(tp_download_write(&io,256,f.expected+256,256)==TP_OK);
    f.verify_error=true; assert(tp_download_finish(&io)==TP_IO_ERROR);
    f.verify_error=false; f.banks[1][0]^=1; assert(tp_download_finish(&io)==TP_INVALID);
    assert(tp_download_read(&io,&s)==TP_OK && s.phase==TP_DOWNLOAD_WRITING);
    return 0;
}
