#include "tp_trial_confirm.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t slots[2][128]; bool fail_read,fail_verify,fail_after_commit,fail_next_read; } fake;
static tp_result rd(void *p,unsigned slot,uint32_t off,void *out,size_t n) {
    fake *f=p; if(f->fail_read || f->fail_next_read) { f->fail_next_read=false; return TP_IO_ERROR; }
    assert(slot<2 && off+n<=128); memcpy(out,f->slots[slot]+off,n);
    return TP_OK;
}
static tp_result erase(void *p,unsigned slot) { fake *f=p; memset(f->slots[slot],0xff,128); return TP_OK; }
static tp_result wr(void *p,unsigned slot,uint32_t off,const void *in,size_t n) { fake *f=p; memcpy(f->slots[slot]+off,in,n); if(f->fail_after_commit && off==32 && n==4) { f->fail_after_commit=false; f->fail_next_read=true; } return TP_OK; }
static tp_result verify(void *p,uint8_t bank,uint32_t v) { fake *f=p; return f->fail_verify ? TP_IO_ERROR : ((bank==0 && v==1)||(bank==1 && v==2) ? TP_OK : TP_INVALID); }
static tp_boot_io io(fake *f) { return (tp_boot_io){{f,rd,erase,wr},f,verify}; }
static void trial(fake *f,tp_boot_io *boot) {
    memset(f,0,sizeof(*f)); memset(f->slots,0xff,sizeof(f->slots)); *boot=io(f);
    assert(tp_boot_provision(boot,0,1)==TP_OK && tp_boot_stage(boot,1,2)==TP_OK);
    uint8_t bank; assert(tp_boot_select(boot,&bank)==TP_OK && bank==1);
}
int main(void) {
    fake f; tp_boot_io boot; tp_trial_confirm g; tp_boot_state state;
    trial(&f,&boot);
    assert(tp_trial_confirm_init(&g,&boot,1,0,1)==TP_INVALID);
    assert(tp_trial_confirm_init(&g,&boot,1,100,40)==TP_OK);
    assert(tp_trial_confirm_poll(&g,0xfffffff0u,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x10,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x30,false)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x40,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x80,true)==TP_BUSY); /* gap resets */
    assert(tp_trial_confirm_poll(&g,0xa0,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0xc0,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0xe0,true)==TP_BUSY);
    f.fail_verify=true; assert(tp_trial_confirm_poll(&g,0x100,true)==TP_IO_ERROR); f.fail_verify=false;
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_TRIAL);
    assert(tp_trial_confirm_poll(&g,0x110,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x130,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x150,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x170,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&g,0x190,true)==TP_OK);
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_IDLE && state.active_bank==1);
    assert(tp_trial_confirm_poll(&g,0x200,false)==TP_OK);
    trial(&f,&boot); assert(tp_trial_confirm_init(&g,&boot,0,10,10)==TP_OK);
    assert(tp_trial_confirm_poll(&g,0,true)==TP_INVALID);
    assert(tp_trial_confirm_init(&g,&boot,1,10,10)==TP_OK);
    assert(tp_trial_confirm_poll(&g,0,true)==TP_BUSY); f.fail_read=true;
    assert(tp_trial_confirm_poll(&g,5,true)==TP_IO_ERROR); f.fail_read=false;
    assert(tp_trial_confirm_poll(&g,10,true)==TP_BUSY);
    f.fail_after_commit=true;
    assert(tp_trial_confirm_poll(&g,20,true)==TP_IO_ERROR);
    /* The commit succeeded even though its readback failed. */
    assert(tp_trial_confirm_poll(&g,21,true)==TP_OK);
    assert(tp_boot_read(&boot,&state)==TP_OK && state.active_bank==1 && state.phase==TP_BOOT_IDLE);
    return 0;
}
