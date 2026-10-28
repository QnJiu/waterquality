#include "tp_upgrade_stream.h"
#include <assert.h>
#include <string.h>

static unsigned executed, sent;
static bool send_fail;
static uint8_t response[120];
static size_t response_size;
/* Exercise the real framing and stream code with a deterministic command. */
tp_result tp_upgrade_command_execute(const tp_upgrade_commands *commands,
    tp_upgrade_command command, const uint8_t *data, size_t size, tp_upgrade_reply *reply) {
    (void)commands; (void)data;
    assert(command == TP_UP_CMD_STATUS && size == 0);
    executed++;
    memset(reply, 0, sizeof(*reply));
    reply->result = TP_OK;
    return TP_OK;
}
static tp_result send_frame(void *ctx, const uint8_t *p, size_t n) {
    (void)ctx; sent++;
    assert(n <= sizeof(response));
    memcpy(response, p, n); response_size = n;
    return send_fail ? TP_IO_ERROR : TP_OK;
}
static tp_result feed(tp_upgrade_stream *s, const uint8_t *p, size_t n, uint32_t at) {
    tp_result r = TP_OK;
    for (size_t i=0; i<n; ++i) r = tp_upgrade_stream_byte(s, p[i], at);
    return r;
}
int main(void) {
    tp_upgrade_commands commands = {0};
    tp_upgrade_stream s;
    uint8_t frame[TP_UP_WIRE_ENCODED_MAX]; size_t n;
    assert(tp_upgrade_stream_init(&s,&commands,NULL,send_frame,0)==TP_INVALID);
    assert(tp_upgrade_stream_init(&s,&commands,NULL,send_frame,0x80000000u)==TP_INVALID);
    assert(tp_upgrade_stream_init(&s,&commands,NULL,send_frame,50)==TP_OK);
    assert(tp_upgrade_wire_request(TP_UP_CMD_STATUS,0x7e7d,NULL,0,frame,sizeof(frame),&n)==TP_OK);
    assert(feed(&s,frame,n,1)==TP_OK && executed==1 && sent==1);
    assert(response_size>2 && response[0]==0x7e && response[response_size-1]==0x7e);
    /* Split at a timeout: the trailing bytes alone must not execute. */
    assert(feed(&s,frame,n/2,10)==TP_OK);
    assert(feed(&s,frame+n/2,n-n/2,60)==TP_OK && executed==1);
    assert(feed(&s,frame,n,61)==TP_OK && executed==2);
    /* Explicit disconnect clears the partial frame, as does idle polling. */
    assert(feed(&s,frame,n/2,70)==TP_OK);
    tp_upgrade_stream_reset(&s);
    assert(feed(&s,frame+n/2,n-n/2,71)==TP_OK && executed==2);
    assert(feed(&s,frame,n/2,80)==TP_OK);
    tp_upgrade_stream_poll(&s,130);
    assert(feed(&s,frame+n/2,n-n/2,131)==TP_OK && executed==2);
    /* Millisecond wrap preserves an unexpired frame and expires a late one. */
    tp_upgrade_stream_reset(&s);
    assert(feed(&s,frame,n/2,0xfffffff0u)==TP_OK);
    assert(feed(&s,frame+n/2,n-n/2,5)==TP_OK && executed==3);
    assert(feed(&s,frame,n/2,0xfffffff0u)==TP_OK);
    assert(feed(&s,frame+n/2,n-n/2,40)==TP_OK && executed==3);
    send_fail=true;
    assert(feed(&s,frame,n,41)==TP_IO_ERROR && executed==4 && sent==4);
    return 0;
}
