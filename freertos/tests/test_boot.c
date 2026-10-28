#include "tp_boot.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t slots[2][64]; uint32_t version[2]; int op,fail; bool image_io; } fake;
static bool fail(fake *f) { return ++f->op==f->fail; }
static tp_result read_slot(void *p,unsigned slot,uint32_t off,void *out,size_t n) {
    fake *f=p; if(fail(f)) return TP_IO_ERROR; assert(slot<2 && off+n<=64); memcpy(out,f->slots[slot]+off,n); return TP_OK;
}
static tp_result erase(void *p,unsigned slot) {
    fake *f=p; if(fail(f)) return TP_IO_ERROR; assert(slot<2); memset(f->slots[slot],0xff,64); return TP_OK;
}
static tp_result write_slot(void *p,unsigned slot,uint32_t off,const void *in,size_t n) {
    fake *f=p; assert(slot<2 && off+n<=64); if(fail(f)) { memcpy(f->slots[slot]+off,in,n/2); return TP_IO_ERROR; }
    for(size_t i=0;i<n;i++) { assert((f->slots[slot][off+i] & ((const uint8_t *)in)[i])==((const uint8_t *)in)[i]); f->slots[slot][off+i]&=((const uint8_t *)in)[i]; }
    return TP_OK;
}
static tp_result verify(void *p,uint8_t bank,uint32_t version) {
    fake *f=p; if(f->image_io) return TP_IO_ERROR; return bank<2 && f->version[bank]==version ? TP_OK : TP_INVALID;
}
static tp_boot_io make_io(fake *f) { tp_boot_io io={{f,read_slot,erase,write_slot},f,verify}; return io; }
static void init(fake *f) { memset(f,0,sizeof(*f)); memset(f->slots,0xff,sizeof(f->slots)); f->version[0]=1; f->version[1]=2; }
int main(void) {
    fake f; init(&f); tp_boot_io io=make_io(&f); tp_boot_state s; uint8_t bank=99;
    assert(tp_boot_provision(&io,0,1)==TP_OK);
    assert(tp_boot_provision(&io,0,1)==TP_BUSY);
    assert(tp_boot_stage(&io,0,2)==TP_INVALID && tp_boot_stage(&io,1,1)==TP_INVALID);
    assert(tp_boot_stage(&io,1,2)==TP_OK);
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_READY && s.active_bank==0);
    assert(tp_boot_select(&io,&bank)==TP_OK && bank==1);
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_TRIAL);
    assert(tp_boot_confirm(&io,0)==TP_INVALID);
    assert(tp_boot_select(&io,&bank)==TP_OK && bank==0);
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_IDLE && s.active_version==1);
    assert(tp_boot_stage(&io,1,2)==TP_OK && tp_boot_select(&io,&bank)==TP_OK && bank==1);
    assert(tp_boot_confirm(&io,1)==TP_OK && tp_boot_select(&io,&bank)==TP_OK && bank==1);
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_IDLE && s.active_version==2);
    init(&f); io=make_io(&f); f.slots[0][0]=0; assert(tp_boot_provision(&io,0,1)==TP_INVALID);
    init(&f); io=make_io(&f); assert(tp_boot_provision(&io,0,1)==TP_OK);
    assert(tp_boot_stage(&io,1,2)==TP_OK); f.version[1]=0;
    assert(tp_boot_select(&io,&bank)==TP_OK && bank==0);
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_IDLE);
    f.version[1]=2; assert(tp_boot_stage(&io,1,2)==TP_OK); f.image_io=true; bank=99;
    assert(tp_boot_select(&io,&bank)==TP_IO_ERROR && bank==99); f.image_io=false;
    assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_READY);
    /* Interrupt each metadata operation in staging. The prior confirmed record
     * remains usable; a fully committed candidate may also be observed. */
    for(int at=1;at<=7;at++) {
        init(&f); io=make_io(&f); assert(tp_boot_provision(&io,0,1)==TP_OK);
        f.op=0; f.fail=at; assert(tp_boot_stage(&io,1,2)==TP_IO_ERROR);
        f.fail=0; assert(tp_boot_read(&io,&s)==TP_OK && s.active_bank==0 && s.active_version==1);
        assert(s.phase==TP_BOOT_IDLE || s.phase==TP_BOOT_READY);
        assert(tp_boot_select(&io,&bank)==TP_OK);
        if(bank==1) assert(tp_boot_select(&io,&bank)==TP_OK && bank==0);
        assert(bank==0);
    }
    /* A failed trial-state commit never returns a bank. On reboot either the
     * candidate is first tried, or an already committed trial rolls back. */
    for(int at=1;at<=7;at++) {
        init(&f); io=make_io(&f); assert(tp_boot_provision(&io,0,1)==TP_OK);
        assert(tp_boot_stage(&io,1,2)==TP_OK);
        f.op=0; f.fail=at; bank=99; assert(tp_boot_select(&io,&bank)==TP_IO_ERROR && bank==99);
        f.fail=0; assert(tp_boot_read(&io,&s)==TP_OK && s.active_bank==0);
        assert(tp_boot_select(&io,&bank)==TP_OK);
        if(bank==1) assert(tp_boot_select(&io,&bank)==TP_OK && bank==0);
        assert(bank==0);
    }
    /* Failed confirmation can have reached persistent storage; both outcomes
     * are safe: confirmed new image or rollback to verified old image. */
    for(int at=1;at<=7;at++) {
        init(&f); io=make_io(&f); assert(tp_boot_provision(&io,0,1)==TP_OK);
        assert(tp_boot_stage(&io,1,2)==TP_OK && tp_boot_select(&io,&bank)==TP_OK && bank==1);
        f.op=0; f.fail=at; assert(tp_boot_confirm(&io,1)==TP_IO_ERROR);
        f.fail=0; assert(tp_boot_select(&io,&bank)==TP_OK);
        assert(bank==0 || bank==1);
        assert(tp_boot_read(&io,&s)==TP_OK && s.phase==TP_BOOT_IDLE && s.active_bank==bank);
    }
    return 0;
}
