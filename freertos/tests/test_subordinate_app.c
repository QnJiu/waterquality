#include "tp_subordinate_app.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t uart[TP_SUB_RESPONSE_SIZE], rx[TP_PN532_FRAME_MAX];
    size_t uart_len, rx_len;
    bool unlocked; uint32_t request_id; unsigned requests;
} fake;
static tp_result uart_send(void *ctx,const uint8_t *p,size_t n) {
    fake *f=ctx; assert(n<=sizeof(f->uart)); memcpy(f->uart,p,n); f->uart_len=n; return TP_OK;
}
static tp_result inputs(void *ctx,tp_sub_inputs *out) { (void)ctx; *out=(tp_sub_inputs){0}; return TP_OK; }
static tp_result pn_send(void *ctx,const uint8_t *p,size_t n) { (void)ctx; (void)p; (void)n; return TP_OK; }
static tp_result pn_receive(void *ctx,uint8_t *p,size_t cap,size_t *n) {
    fake *f=ctx; if(!f->rx_len) return TP_BUSY;
    assert(cap>=f->rx_len); memcpy(p,f->rx,f->rx_len); *n=f->rx_len; f->rx_len=0; return TP_OK;
}
static tp_result set_lock(void *ctx,bool unlocked) { ((fake *)ctx)->unlocked=unlocked; return TP_OK; }
static tp_result request(void *ctx,uint32_t id,const uint8_t *uid,size_t n) {
    fake *f=ctx; assert(n==4 && uid[0]==0xab); f->request_id=id; f->requests++; return TP_OK;
}
static void ack(fake *f) { const uint8_t p[]={0,0,0xff,0,0xff,0}; memcpy(f->rx,p,sizeof(p)); f->rx_len=sizeof(p); }
static void response(fake *f,uint8_t command,const uint8_t *data,size_t n) {
    uint8_t *p=f->rx; p[0]=0; p[1]=0; p[2]=0xff; p[3]=(uint8_t)(n+2); p[4]=(uint8_t)(0-p[3]);
    p[5]=0xd5; p[6]=(uint8_t)(command+1); uint8_t sum=(uint8_t)(p[5]+p[6]);
    for(size_t i=0;i<n;i++) { p[7+i]=data[i]; sum=(uint8_t)(sum+data[i]); }
    p[7+n]=(uint8_t)(0-sum); p[8+n]=0; f->rx_len=n+9;
}
static void step(tp_sub_app *app,uint32_t ms) { assert(tp_sub_app_tick(app,ms*1000u,ms)==TP_OK); }
int main(void) {
    fake f={0}; tp_door_service door; tp_sub_app app; uint32_t budget;
    tp_pn532_io reader={&f,pn_send,pn_receive}; tp_door_io lock={&f,set_lock};
    tp_sub_node_io io={&f,uart_send,inputs};
    assert(tp_door_service_init(&door,&reader,&lock,&f,request,100,50,2,40,200,7)==TP_OK);
    assert(tp_sub_app_init(&app,&io,115200,10,&door,true)==TP_OK);
    step(&app,0); assert(app.door_result==TP_BUSY);
    assert(!tp_sub_app_sleep_budget(&app,0,100,true,&budget));
    ack(&f); step(&app,1); response(&f,0x14,NULL,0); step(&app,2);
    ack(&f); step(&app,3); response(&f,0x32,NULL,0); step(&app,4);
    ack(&f); step(&app,5);
    const uint8_t card[]={1,1,4,0,8,4,0xab,0xcd,0xef,0x12};
    response(&f,0x4a,card,sizeof(card)); step(&app,6);
    assert(f.requests==1 && door.door.auth_pending);
    assert(!tp_sub_app_sleep_budget(&app,6,100,true,&budget));
    assert(tp_door_service_authorize(&door,7,f.request_id,true)==TP_OK && f.unlocked);
    assert(!tp_sub_app_sleep_budget(&app,7,100,true,&budget));
    step(&app,47); assert(!f.unlocked);
    assert(tp_sub_app_sleep_budget(&app,47,100,true,&budget) && budget==9);
    assert(!tp_sub_app_sleep_budget(&app,47,100,false,&budget));
    assert(tp_sub_node_byte(&app.node,0x12,48000,48)==TP_OK);
    assert(!tp_sub_app_sleep_budget(&app,48,100,true,&budget));
    assert(tp_sub_node_error(&app.node,49000,49)==TP_OK); step(&app,52);
    assert(tp_sub_app_sleep_budget(&app,52,100,true,&budget) && budget==4);
    step(&app,56); step(&app,156);
    assert(app.door_result==TP_TIMEOUT && door.door.state==TP_DOOR_FAULT);
    assert(!tp_sub_app_sleep_budget(&app,156,100,true,&budget));

    tp_sub_master master; uint8_t command[TP_SUB_REQUEST_SIZE];
    assert(tp_sub_master_init(&master,20,100,1)==TP_OK);
    assert(tp_sub_master_request(&master,160,command)==TP_OK);
    tp_sub_node_droplet(&app.node);
    for(size_t i=0;i<sizeof(command);i++)
        assert(tp_sub_node_byte(&app.node,command[i],160000+(uint32_t)i*100,160)==TP_OK);
    step(&app,164);
    assert(tp_sub_master_receive(&master,164,f.uart,f.uart_len)==TP_OK);
    assert(master.status.droplet_count==1 && master.status.faults==TP_SUB_DEVICE_FAULT);
    assert(app.door_result==TP_IO_ERROR);
    return 0;
}
