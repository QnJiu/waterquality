#include "tp_upgrade.h"

static tp_result eligible(const tp_upgrade *u, const tp_download_manifest *m) {
    if(!u || !u->boot || !u->download || !m) return TP_INVALID;
    tp_boot_state boot;
    tp_result r=tp_boot_read(u->boot,&boot);
    if(r!=TP_OK) return r;
    if(boot.phase!=TP_BOOT_IDLE) return TP_BUSY;
    if(m->bank>1 || m->bank==boot.active_bank || m->version<=boot.active_version)
        return TP_INVALID;
    return TP_OK;
}

static tp_result current(const tp_upgrade *u, tp_download_state *s) {
    if(!u || !u->boot || !u->download || !s) return TP_INVALID;
    tp_result r=tp_download_read(u->download,s);
    if(r!=TP_OK) return r;
    return eligible(u,&s->manifest);
}

tp_result tp_upgrade_begin(const tp_upgrade *u, const tp_download_manifest *m) {
    tp_result r=eligible(u,m);
    return r==TP_OK ? tp_download_begin(u->download,m) : r;
}

tp_result tp_upgrade_resume(const tp_upgrade *u) {
    tp_download_state s;
    tp_result r=current(u,&s);
    return r==TP_OK ? tp_download_resume(u->download) : r;
}

tp_result tp_upgrade_write(const tp_upgrade *u, uint32_t offset, const void *data, size_t size) {
    tp_download_state s;
    tp_result r=current(u,&s);
    return r==TP_OK ? tp_download_write(u->download,offset,data,size) : r;
}

tp_result tp_upgrade_finish(const tp_upgrade *u) {
    if(!u || !u->boot || !u->download) return TP_INVALID;
    tp_download_state download;
    tp_result r=tp_download_read(u->download,&download);
    if(r!=TP_OK) return r;
    tp_boot_state boot;
    r=tp_boot_read(u->boot,&boot);
    if(r!=TP_OK) return r;
    if(boot.phase==TP_BOOT_READY && download.phase==TP_DOWNLOAD_VERIFIED &&
       boot.candidate_bank==download.manifest.bank &&
       boot.candidate_version==download.manifest.version) return TP_OK;
    if(boot.phase!=TP_BOOT_IDLE) return TP_BUSY;
    if(download.manifest.bank==boot.active_bank || download.manifest.version<=boot.active_version)
        return TP_INVALID;
    r=tp_download_finish(u->download);
    if(r!=TP_OK) return r;
    return tp_boot_stage(u->boot,download.manifest.bank,download.manifest.version);
}
