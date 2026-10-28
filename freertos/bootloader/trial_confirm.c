#include "tp_trial_confirm.h"
#include <string.h>
tp_result tp_trial_confirm_init(tp_trial_confirm *g,const tp_boot_io *boot,uint8_t bank,uint32_t stable_ms,uint32_t max_gap_ms) {
    if(!g || !boot || !boot->metadata.read || !boot->metadata.erase || !boot->metadata.write ||
       !boot->verify_image || bank>1 || !stable_ms || stable_ms>=0x80000000u || !max_gap_ms || max_gap_ms>stable_ms) return TP_INVALID;
    memset(g,0,sizeof(*g)); g->boot=*boot; g->running_bank=bank; g->stable_ms=stable_ms; g->max_gap_ms=max_gap_ms;
    return TP_OK;
}
tp_result tp_trial_confirm_poll(tp_trial_confirm *g,uint32_t now,bool healthy) {
    if(!g || !g->stable_ms || g->running_bank>1) return TP_INVALID;
    if(g->complete) return TP_OK;
    tp_boot_state s;
    tp_result r=tp_boot_read(&g->boot,&s);
    if(r!=TP_OK) { g->timing=false; return r; }
    if(s.phase==TP_BOOT_IDLE) {
        if(s.active_bank!=g->running_bank) { g->timing=false; return TP_INVALID; }
        g->complete=true; return TP_OK;
    }
    if(s.phase!=TP_BOOT_TRIAL || s.candidate_bank!=g->running_bank) { g->timing=false; return TP_INVALID; }
    if(!healthy) { g->timing=false; return TP_BUSY; }
    if(g->timing && (uint32_t)(now-g->last_sample)>g->max_gap_ms) g->timing=false;
    g->last_sample=now;
    if(!g->timing) { g->healthy_since=now; g->timing=true; return TP_BUSY; }
    if((uint32_t)(now-g->healthy_since)<g->stable_ms) return TP_BUSY;
    r=tp_boot_confirm(&g->boot,g->running_bank);
    if(r==TP_OK) g->complete=true;
    else g->timing=false; /* require a new stable period after an uncertain result */
    return r;
}
