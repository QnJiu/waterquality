#include "tp_event_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t mem[128]; int calls, fail_call, ignore_call; size_t partial; bool read_fail; } fake;
static tp_result read_mem(void *ctx,uint32_t off,void *out,size_t n) {
    fake *f=ctx; if(f->read_fail || off+n>sizeof(f->mem)) return TP_IO_ERROR;
    memcpy(out,f->mem+off,n); return TP_OK;
}
static tp_result write_mem(void *ctx,uint32_t off,const void *src,size_t n) {
    fake *f=ctx; if(off+n>sizeof(f->mem)) return TP_IO_ERROR;
    if(f->calls==f->ignore_call) { f->calls++; return TP_OK; }
    if(f->calls++==f->fail_call) { if(f->partial<n) n=f->partial; memcpy(f->mem+off,src,n); return TP_IO_ERROR; }
    memcpy(f->mem+off,src,n); return TP_OK;
}
static void reset(fake *f) { memset(f,0,sizeof(*f)); memset(f->mem,0xff,sizeof(f->mem)); f->fail_call=-1; f->ignore_call=-1; }
int main(void) {
    fake f; reset(&f); tp_event_log_io io={&f,read_mem,write_mem,sizeof(f.mem)};
    tp_event_log log; tp_event e={.type=7,.detail=3,.value=42,.utc_seconds=-123,.utc_valid=true}, out;
    assert(tp_event_log_open(&log,&io)==TP_OK && !log.has_records);
    for(unsigned i=1;i<=6;i++) { e.value=i; assert(tp_event_log_append(&log,&e)==TP_OK && e.sequence==i); }
    assert(tp_event_log_read(&log,2,&out)==TP_INVALID);
    assert(tp_event_log_read(&log,3,&out)==TP_OK && out.value==3 && out.utc_seconds==-123 && out.utc_valid);
    assert(tp_event_log_open(&log,&io)==TP_OK && log.latest_sequence==6);
    f.mem[2*32+4]^=1; /* corrupt seq 3 without changing CRC */
    assert(tp_event_log_read(&log,3,&out)==TP_INVALID);
    for(int failure=0;failure<3;failure++) {
        fake g; reset(&g); tp_event_log_io gi={&g,read_mem,write_mem,sizeof(g.mem)}; tp_event_log l;
        assert(tp_event_log_open(&l,&gi)==TP_OK); assert(tp_event_log_append(&l,&e)==TP_OK);
        g.calls=0; g.fail_call=failure; g.partial=(failure==1 ? 13 : 1);
        assert(tp_event_log_append(&l,&e)==TP_IO_ERROR && !l.open);
        g.fail_call=-1; assert(tp_event_log_open(&l,&gi)==TP_OK);
        /* A reported error on the one-byte commit may still mean committed. */
        assert(l.latest_sequence==(failure==2 ? 2u : 1u));
        assert(tp_event_log_read(&l,1,&out)==TP_OK);
        assert(tp_event_log_append(&l,&e)==TP_OK);
    }
    /* A device that reports success but ignores writes must not publish an
     * unverified replacement or report an uncommitted event as saved. */
    for(int ignored=0;ignored<=2;ignored++) {
        fake g; reset(&g); tp_event_log_io gi={&g,read_mem,write_mem,sizeof(g.mem)}; tp_event_log l;
        assert(tp_event_log_open(&l,&gi)==TP_OK);
        for(unsigned i=0;i<4;i++) assert(tp_event_log_append(&l,&e)==TP_OK);
        g.calls=0; g.ignore_call=ignored;
        assert(tp_event_log_append(&l,&e)==TP_IO_ERROR && !l.open);
        g.ignore_call=-1;
        assert(tp_event_log_open(&l,&gi)==TP_OK && l.latest_sequence==4);
        if(ignored==0) assert(tp_event_log_read(&l,1,&out)==TP_OK);
        else assert(tp_event_log_read(&l,1,&out)==TP_INVALID);
        assert(tp_event_log_append(&l,&e)==TP_OK && e.sequence==5);
    }
    reset(&f); assert(tp_event_log_open(&log,&io)==TP_OK);
    log.has_records=true; log.latest_slot=3; log.latest_sequence=0xfffffffeu;
    assert(tp_event_log_append(&log,&e)==TP_OK && e.sequence==0xffffffffu);
    assert(tp_event_log_append(&log,&e)==TP_OK && e.sequence==0);
    assert(tp_event_log_open(&log,&io)==TP_OK && log.latest_sequence==0);
    f.read_fail=true; assert(tp_event_log_read(&log,0,&out)==TP_IO_ERROR && !log.open);
    puts("event log recovery checks passed"); return 0;
}
