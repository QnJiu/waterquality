#define main signed_boot_original_main
#include "test_signed_boot.c"
#undef main
#include "tp_staged_upgrade.h"
static const char *signed_v1="5450553178563412020000000300000001000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015adad628c012e2dd70b93c667ef7166ff7dbf199f87e4105fbf15f89ba483f0e7f169b5287eedeef89d4a6299c0fc066a9a94b7418f92722a08f81f22d5b9f12679";
typedef struct { fake target; tp_signed_boot boot; uint8_t source[3]; unsigned erases; } fixture;
static tp_result source_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(out,f->source+off,n); return TP_OK;
}
static tp_result dest_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; return read_image(&f->target,bank,off,out,n);
}
static tp_result dest_erase(void *v,uint8_t bank) {
    fixture *f=v; if(bank!=1) return TP_INVALID;
    f->erases++; memset(f->target.image[bank],0xff,3); return TP_OK;
}
static tp_result dest_write(void *v,uint8_t bank,uint32_t off,const void *in,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(f->target.image[bank]+off,in,n); return TP_OK;
}
#include "tp_stage_receiver.h"
#include "tp_stage_commands.h"
#include "tp_upgrade_stream.h"
static uint8_t response[TP_UP_WIRE_ENCODED_MAX];
static size_t response_len;
static tp_result capture(void *ctx,const uint8_t *data,size_t n) {
    (void)ctx; assert(n<=sizeof(response)); memcpy(response,data,n); response_len=n; return TP_OK;
}
static void send_command(tp_upgrade_stream *stream,uint8_t cmd,const uint8_t *payload,size_t n) {
    uint8_t frame[TP_UP_WIRE_ENCODED_MAX]; size_t length;
    assert(tp_upgrade_wire_request(cmd,42,payload,n,frame,sizeof(frame),&length)==TP_OK);
    response_len=0;
    for(size_t i=0;i<length;i++) assert(tp_upgrade_stream_byte(stream,frame[i],1)==TP_OK);
    assert(response_len>10 && response[0]==0x7e && response[response_len-1]==0x7e);
    assert(response[7]==TP_OK);
}
static tp_result stage_erase(void *v,uint8_t bank) {
    fixture *f=v; if(bank!=1) return TP_INVALID;
    f->erases++; memset(f->source,0xff,3); return TP_OK;
}
static tp_result stage_write(void *v,uint8_t bank,uint32_t off,const void *p,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(f->source+off,p,n); return TP_OK;
}
int main(void) {
    fixture f; init(&f.target,&f.boot); f.erases=0;
    fake boot_store,stage_meta; tp_signed_boot unused; init(&boot_store,&unused); init(&stage_meta,&unused);
    tp_secure_upgrade_config c={0};
    c.boot_metadata=(tp_store_io){&boot_store,read_meta,erase_meta,write_meta};
    c.download_metadata=(tp_store_io){&f.target,read_meta,erase_meta,write_meta};
    c.manifests=(tp_store_io){&f.target,read_manifest,erase_manifest,write_manifest};
    c.image_ctx=&f; c.erase_image=dest_erase; c.read_image=dest_read; c.write_image=dest_write;
    c.capacity[0]=c.capacity[1]=3; c.product_id=f.boot.signature.product_id;
    memcpy(c.public_key,f.boot.signature.public_key,64);
    tp_secure_upgrade u; assert(tp_secure_upgrade_init(&u,&c)==TP_OK);
    const char *v0="5450553178563412010000000300000000000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad951584689bde05b6bcb46ada8a291c99c8636798711a92c0b11b3da52d664e42a40bb63cca6e220774961b8f1eb8b1a1b71ec31c225c163e8a3c24e8003fb2d7";
    uint8_t b0[116],b1[116]; hex(v0,b0,116); hex(signed_v1,b1,116);
    tp_download_manifest m; assert(tp_signed_boot_authenticate(&u.signed_boot,b0,116,&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&u.signed_boot,&m)==TP_OK);
    assert(tp_boot_provision(&u.boot,0,1)==TP_OK);
    tp_download_io storage={{&stage_meta,read_meta,erase_meta,write_meta},&f,stage_erase,source_read,stage_write,NULL};
    tp_image_hash_io image={&f,source_read,{3,3}}; tp_stage_receiver s;
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b0,116)==TP_INVALID && f.erases==0);
    b1[52]^=1;
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_INVALID && f.erases==0);
    b1[52]^=1;
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id+1,b1,116)==TP_INVALID && f.erases==0);
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_OK && f.erases==1);
    assert(tp_stage_receiver_write(&s,0,"a",1)==TP_OK);
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_OK && f.erases==1);
    assert(tp_stage_receiver_write(&s,1,"bc",2)==TP_OK);
    assert(tp_stage_receiver_finish(&s)==TP_OK);
    f.source[0]^=1;
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_INVALID && f.erases==1);
    f.source[0]^=1;
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_OK);
    /* Exercise real framing and the resumable staging command endpoint. */
    init(&stage_meta,&unused); f.erases=0;
    tp_stage_commands commands; tp_upgrade_commands placeholder={0}; tp_upgrade_stream stream;
    assert(tp_stage_commands_init(&commands,&u.boot,&storage,image.capacity,c.public_key,c.product_id)==TP_OK);
    assert(tp_upgrade_stream_init(&stream,&placeholder,NULL,capture,50)==TP_OK);
    tp_upgrade_wire_set_executor(&stream.wire,&commands,tp_stage_commands_execute);
    send_command(&stream,TP_UP_CMD_BEGIN,b1,116);
    uint8_t first[]={0,0,0,0,'a'}; send_command(&stream,TP_UP_CMD_WRITE,first,sizeof(first));
    tp_upgrade_reply bad_reply; uint8_t wrong[]={0,0,0,0,'z'};
    assert(tp_stage_commands_execute(&commands,NULL,TP_UP_CMD_WRITE,wrong,sizeof(wrong),&bad_reply)==TP_INVALID);
    assert(bad_reply.state_available && bad_reply.state.written==1);
    assert(tp_stage_commands_init(&commands,&u.boot,&storage,image.capacity,c.public_key,c.product_id)==TP_OK);
    send_command(&stream,TP_UP_CMD_BEGIN,b1,116);
    tp_upgrade_reply reply;
    assert(tp_stage_commands_execute(&commands,NULL,TP_UP_CMD_STATUS,NULL,0,&reply)==TP_OK);
    assert(reply.state_available && reply.state.written==1 && f.erases==1);
    uint8_t rest[]={1,0,0,0,'b','c'}; send_command(&stream,TP_UP_CMD_WRITE,rest,sizeof(rest));
    send_command(&stream,TP_UP_CMD_FINISH,NULL,0);
    assert(tp_stage_commands_execute(&commands,NULL,TP_UP_CMD_STATUS,NULL,0,&reply)==TP_OK);
    assert(reply.state.phase==TP_DOWNLOAD_VERIFIED);
    f.source[0]^=1;
    assert(tp_stage_commands_execute(&commands,NULL,TP_UP_CMD_FINISH,NULL,0,&reply)==TP_INVALID);
    f.source[0]^=1;
    send_command(&stream,TP_UP_CMD_BEGIN,b1,116);
    tp_staged_upgrade upgrade; assert(tp_staged_upgrade_init(&upgrade,&u,&image,b1,116)==TP_OK);
    assert(tp_staged_upgrade_step(&upgrade)==TP_BUSY);
    assert(tp_staged_upgrade_step(&upgrade)==TP_OK);
    assert(tp_staged_upgrade_finish(&upgrade)==TP_OK);
    assert(tp_stage_receiver_open(&s,&u.boot,&storage,image.capacity,c.public_key,c.product_id,b1,116)==TP_BUSY);
    return 0;
}
