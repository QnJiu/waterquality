#include "tp_rtu_receiver.h"
#include <assert.h>
#include <stdio.h>
static int frames; static size_t bytes;
static void frame(void *ctx, const uint8_t *b, size_t n) { (void)ctx; (void)b; frames++; bytes=n; }
int main(void) {
    tp_rtu_receiver r; assert(tp_rtu_receiver_init(&r,9600,11,NULL,frame)==TP_OK);
    tp_rtu_receiver_byte(&r,1,0xfffffff0u); tp_rtu_receiver_byte(&r,2,100);
    tp_rtu_receiver_poll(&r,100+r.gap_3_5_us+r.char_us-1); assert(frames==0);
    tp_rtu_receiver_poll(&r,100+r.gap_3_5_us+r.char_us); assert(frames==1 && bytes==2);
    tp_rtu_receiver_byte(&r,1,10000); tp_rtu_receiver_byte(&r,2,10001+r.gap_1_5_us+r.char_us);
    tp_rtu_receiver_poll(&r,10001+r.gap_1_5_us+r.char_us+r.gap_3_5_us+r.char_us); assert(frames==1);
    tp_rtu_receiver_byte(&r,1,30000); tp_rtu_receiver_error(&r,30001);
    tp_rtu_receiver_poll(&r,30001+r.gap_3_5_us+r.char_us); assert(frames==1);
    for (unsigned i=0;i<257;i++) tp_rtu_receiver_byte(&r,1,40000+i);
    tp_rtu_receiver_poll(&r,40256+r.gap_3_5_us+r.char_us); assert(frames==1);
    puts("RTU receiver checks passed"); return 0;
}

