#include "tp_upgrade.h"
#include "tp_upgrade_commands.h"
#include "tp_upgrade_wire.h"
#include <assert.h>
#include <string.h>

typedef struct { uint8_t slot[2][128]; bool fail_commit_read, committed; } store;
typedef struct { uint8_t bank[2][16]; unsigned erases, writes; bool reject; } images;
static tp_result read_slot(void *ctx,unsigned slot,uint32_t off,void *out,size_t n) {
    store *s=ctx; assert(slot<2 && off+n<=128);
    if(s->fail_commit_read && s->committed && off==0 && n==36) {
        s->fail_commit_read=false; return TP_IO_ERROR;
    }
    memcpy(out,s->slot[slot]+off,n); return TP_OK;
}
static tp_result erase_slot(void *ctx,unsigned slot) {
    store *s=ctx; assert(slot<2); memset(s->slot[slot],0xff,128); return TP_OK;
}
static tp_result write_slot(void *ctx,unsigned slot,uint32_t off,const void *in,size_t n) {
    store *s=ctx; const uint8_t *p=in; assert(slot<2 && off+n<=128);
    for(size_t i=0;i<n;i++) { assert((s->slot[slot][off+i]&p[i])==p[i]); s->slot[slot][off+i]&=p[i]; }
    if(off==32 && n==4) s->committed=true;
    return TP_OK;
}
static tp_result erase_image(void *ctx,uint8_t bank) {
    images *i=ctx; assert(bank<2); i->erases++; memset(i->bank[bank],0xff,16); return TP_OK;
}
static tp_result read_image(void *ctx,uint8_t bank,uint32_t off,void *out,size_t n) {
    images *i=ctx; assert(bank<2 && off+n<=16); memcpy(out,i->bank[bank]+off,n); return TP_OK;
}
static tp_result write_image(void *ctx,uint8_t bank,uint32_t off,const void *in,size_t n) {
    images *i=ctx; assert(bank<2 && off+n<=16); i->writes++; memcpy(i->bank[bank]+off,in,n); return TP_OK;
}
static tp_result verify_download(void *ctx,const tp_download_manifest *m) {
    images *i=ctx;
    return !i->reject && m->bank==1 && m->version==2 && m->size==4 &&
        !memcmp(i->bank[1],"data",4) ? TP_OK : TP_INVALID;
}
static tp_result verify_boot(void *ctx,uint8_t bank,uint32_t version) {
    images *i=ctx;
    if(bank==0 && version==1) return TP_OK;
    return !i->reject && bank==1 && version==2 && !memcmp(i->bank[1],"data",4) ? TP_OK : TP_INVALID;
}
static tp_result authenticate(void *ctx,const uint8_t *data,size_t n,tp_download_manifest *m) {
    (void)ctx;
    if(n!=2 || memcmp(data,"ok",2)) return TP_INVALID;
    *m=(tp_download_manifest){{1},2,4,1}; return TP_OK;
}
typedef struct { uint8_t frame[120]; size_t length; unsigned sends; bool fail; } wire_output;
static tp_result wire_send(void *ctx,const uint8_t *p,size_t n) {
    wire_output *o=ctx; assert(n<=sizeof(o->frame)); memcpy(o->frame,p,n); o->length=n; o->sends++;
    return o->fail ? TP_IO_ERROR : TP_OK;
}
static tp_result deliver(tp_upgrade_wire *w,const uint8_t *p,size_t n) {
    tp_result r=TP_OK;
    for(size_t i=0;i<n;i++) { tp_result next=tp_upgrade_wire_byte(w,p[i]); if(next!=TP_OK) r=next; }
    return r;
}
static size_t unescape(const wire_output *o,uint8_t *out) {
    assert(o->length>=2 && o->frame[0]==0x7e && o->frame[o->length-1]==0x7e);
    size_t n=0;
    for(size_t i=1;i<o->length-1;i++) {
        uint8_t b=o->frame[i]; if(b==0x7d) { assert(++i<o->length-1); b=o->frame[i]^0x20; }
        out[n++]=b;
    }
    return n;
}
static void wire_test(void) {
    store bs={0},ds={0}; images image={0}; memset(bs.slot,0xff,sizeof(bs.slot)); memset(ds.slot,0xff,sizeof(ds.slot));
    tp_boot_io boot={{&bs,read_slot,erase_slot,write_slot},&image,verify_boot};
    tp_download_io download={{&ds,read_slot,erase_slot,write_slot},&image,erase_image,read_image,write_image,verify_download};
    tp_upgrade upgrade={&boot,&download}; tp_upgrade_commands c={&upgrade,NULL,authenticate};
    wire_output output={0}; tp_upgrade_wire wire; uint8_t frame[TP_UP_WIRE_ENCODED_MAX],decoded[120]; size_t n;
    assert(tp_boot_provision(&boot,0,1)==TP_OK && tp_upgrade_wire_init(&wire,&c,&output,wire_send)==TP_OK);
    assert(tp_upgrade_wire_request(TP_UP_CMD_BEGIN,0x7d7e,(const uint8_t *)"ok",2,frame,sizeof(frame),&n)==TP_OK);
    const uint8_t expected[]={0x7e,0x01,0x02,0x7d,0x5e,0x7d,0x5d,0x02,0x00,0x6f,0x6b,0xa9,0xb4,0x7e};
    assert(n==sizeof(expected) && !memcmp(frame,expected,n));
    assert(deliver(&wire,frame,n)==TP_OK && image.erases==1 && output.sends==1);
    assert(unescape(&output,decoded)==56 && decoded[0]==1 && decoded[1]==0x82 && decoded[2]==0x7e && decoded[3]==0x7d);
    assert(decoded[6]==TP_OK && decoded[7]==1 && decoded[8]==TP_DOWNLOAD_WRITING);
    const uint8_t write[]={0,0,0,0,'d','a','t','a'};
    assert(tp_upgrade_wire_request(TP_UP_CMD_WRITE,3,write,sizeof(write),frame,sizeof(frame),&n)==TP_OK);
    frame[n-2]^=1; assert(deliver(&wire,frame,n)==TP_INVALID && !image.writes && output.sends==1);
    assert(tp_upgrade_wire_request(TP_UP_CMD_WRITE,3,write,sizeof(write),frame,sizeof(frame),&n)==TP_OK);
    output.fail=true; assert(deliver(&wire,frame,n)==TP_IO_ERROR && image.writes==1);
    output.fail=false;
    assert(tp_upgrade_wire_request(TP_UP_CMD_STATUS,4,NULL,0,frame,sizeof(frame),&n)==TP_OK);
    assert(deliver(&wire,frame,n)==TP_OK); assert(unescape(&output,decoded)==56 && decoded[18]==4);
    assert(tp_upgrade_wire_request(TP_UP_CMD_FINISH,5,NULL,0,frame,sizeof(frame),&n)==TP_OK);
    assert(deliver(&wire,frame,n)==TP_OK); assert(unescape(&output,decoded)==56 && decoded[6]==TP_OK && decoded[8]==TP_DOWNLOAD_VERIFIED);
    assert(tp_upgrade_wire_byte(&wire,0x7d)==TP_OK);
    assert(tp_upgrade_wire_byte(&wire,0x01)==TP_INVALID);
    assert(tp_upgrade_wire_byte(&wire,0x7e)==TP_INVALID);
    assert(tp_upgrade_wire_request(TP_UP_CMD_STATUS,6,NULL,0,frame,sizeof(frame),&n)==TP_OK);
    assert(deliver(&wire,frame,n)==TP_OK);
    assert(tp_upgrade_wire_byte(&wire,0x42)==TP_OK); tp_upgrade_wire_reset(&wire);
    assert(deliver(&wire,frame,n)==TP_OK);
    for(size_t i=0;i<TP_UP_WIRE_RAW_MAX;i++) assert(tp_upgrade_wire_byte(&wire,0x55)==TP_OK);
    assert(tp_upgrade_wire_byte(&wire,0x55)==TP_INVALID);
    assert(tp_upgrade_wire_byte(&wire,0x7e)==TP_INVALID);
    assert(deliver(&wire,frame,n)==TP_OK);
}
static void commands_test(void) {
    store bs={0},ds={0}; images image={0};
    memset(bs.slot,0xff,sizeof(bs.slot)); memset(ds.slot,0xff,sizeof(ds.slot));
    tp_boot_io boot={{&bs,read_slot,erase_slot,write_slot},&image,verify_boot};
    tp_download_io download={{&ds,read_slot,erase_slot,write_slot},&image,erase_image,read_image,write_image,verify_download};
    tp_upgrade upgrade={&boot,&download}; tp_upgrade_commands c={&upgrade,NULL,authenticate}; tp_upgrade_reply reply;
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_STATUS,NULL,0,&reply)==TP_INVALID && !reply.state_available);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_BEGIN,(const uint8_t *)"no",2,&reply)==TP_INVALID && !image.erases);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_BEGIN,(const uint8_t *)"ok",2,&reply)==TP_OK);
    assert(reply.state_available && reply.state.phase==TP_DOWNLOAD_WRITING && reply.state.written==0);
    const uint8_t first[]={0,0,0,0,'d','a'}, second[]={2,0,0,0,'t','a'};
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_WRITE,second,sizeof(second),&reply)==TP_INVALID && !image.writes);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_WRITE,first,sizeof(first),&reply)==TP_OK && reply.state.written==2);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_STATUS,NULL,0,&reply)==TP_OK && reply.state.written==2);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_RESUME,NULL,0,&reply)==TP_OK);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_WRITE,second,sizeof(second),&reply)==TP_OK && reply.state.written==4);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_FINISH,NULL,0,&reply)==TP_OK && reply.state.phase==TP_DOWNLOAD_VERIFIED);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_FINISH,NULL,0,&reply)==TP_OK);
    tp_boot_state state; assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_READY);
    assert(tp_upgrade_command_execute(&c,TP_UP_CMD_WRITE,first,4,&reply)==TP_INVALID);
}
int main(void) {
    wire_test();
    commands_test();
    store bs,ds; images image={0};
    memset(&bs,0,sizeof(bs)); memset(&ds,0,sizeof(ds));
    memset(bs.slot,0xff,sizeof(bs.slot)); memset(ds.slot,0xff,sizeof(ds.slot));
    tp_boot_io boot={{&bs,read_slot,erase_slot,write_slot},&image,verify_boot};
    tp_download_io download={{&ds,read_slot,erase_slot,write_slot},&image,erase_image,read_image,write_image,verify_download};
    tp_upgrade upgrade={&boot,&download};
    tp_download_manifest manifest={{1},2,4,1};
    assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID);
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    manifest.bank=0; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID && image.erases==0);
    manifest.bank=1; manifest.version=1; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_INVALID && image.erases==0);
    manifest.version=2; assert(tp_upgrade_begin(&upgrade,&manifest)==TP_OK && image.erases==1);
    assert(tp_upgrade_finish(&upgrade)==TP_INVALID);
    assert(tp_upgrade_write(&upgrade,0,"data",4)==TP_OK);
    image.reject=true; assert(tp_upgrade_finish(&upgrade)==TP_INVALID);
    tp_boot_state state; assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_IDLE);
    image.reject=false;
    /* Reset between download verification and boot staging: BEGIN must not
     * erase the already verified image when the client reconnects. */
    assert(tp_download_finish(&download)==TP_OK);
    assert(tp_upgrade_begin(&upgrade,&manifest)==TP_BUSY && image.erases==1);
    tp_download_state download_state;
    assert(tp_download_read(&download,&download_state)==TP_OK && download_state.phase==TP_DOWNLOAD_VERIFIED);
    bs.committed=false; bs.fail_commit_read=true;
    assert(tp_upgrade_finish(&upgrade)==TP_IO_ERROR);
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_READY);
    assert(tp_upgrade_finish(&upgrade)==TP_OK);
    unsigned erases=image.erases, writes=image.writes;
    assert(tp_upgrade_begin(&upgrade,&manifest)==TP_BUSY);
    assert(tp_upgrade_resume(&upgrade)==TP_BUSY);
    assert(tp_upgrade_write(&upgrade,0,"data",4)==TP_BUSY);
    assert(image.erases==erases && image.writes==writes);
    uint8_t bank; assert(tp_boot_select(&boot,&bank)==TP_OK && bank==1);
    assert(tp_upgrade_finish(&upgrade)==TP_BUSY);
    assert(tp_upgrade_resume(&upgrade)==TP_BUSY);
    assert(tp_boot_confirm(&boot,1)==TP_OK);
    assert(tp_upgrade_resume(&upgrade)==TP_INVALID);
    assert(image.erases==erases && image.writes==writes);
    return 0;
}
