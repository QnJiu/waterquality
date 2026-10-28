#include "tp_door_allowlist.h"
#include <string.h>
static bool uid_length(size_t n) { return n==4 || n==7 || n==10; }
tp_result tp_door_allowlist_init(tp_door_allowlist *p,const tp_door_card *cards,size_t count,
                                  void *ctx,tp_door_decision_sink submit) {
    if(!p || (!cards && count) || !submit) return TP_INVALID;
    for(size_t i=0;i<count;i++) if(!uid_length(cards[i].length)) return TP_INVALID;
    *p=(tp_door_allowlist){cards,count,ctx,submit};
    return TP_OK;
}
tp_result tp_door_allowlist_request(void *ctx,uint32_t id,const uint8_t *uid,size_t n) {
    tp_door_allowlist *p=ctx;
    if(!p || !p->submit || !uid || !uid_length(n) || (!p->cards && p->count)) return TP_INVALID;
    bool allowed=false;
    for(size_t i=0;i<p->count;i++) {
        if(p->cards[i].length==n && memcmp(p->cards[i].uid,uid,n)==0) { allowed=true; break; }
    }
    return p->submit(p->ctx,id,allowed);
}
