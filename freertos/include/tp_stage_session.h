#ifndef TP_STAGE_SESSION_H
#define TP_STAGE_SESSION_H
#include "tp_stage_commands.h"
#include "tp_staged_upgrade.h"
/* Single task owns this object and all staging access. The shared lock excludes
 * every other boot, upgrade, confirmation and staging writer until abort or
 * successful installation. Storage and callbacks outlive the session; never
 * reinitialize or move an active session. */
typedef struct {
    tp_stage_commands commands;
    tp_staged_upgrade install;
    tp_secure_upgrade *upgrade;
    tp_image_hash_io image;
    void *ctx;
    bool (*lock)(void *);
    void (*unlock)(void *);
    tp_result (*maintenance)(void *);
    uint8_t manifest[TP_SIGNED_MANIFEST_SIZE];
    bool locked, installing;
} tp_stage_session;
tp_result tp_stage_session_init(tp_stage_session *,tp_secure_upgrade *,const tp_download_io *,
    const uint32_t capacity[2],void *,bool (*lock)(void *),void (*unlock)(void *),
    tp_result (*maintenance)(void *));
/* Executor for tp_upgrade_wire. BEGIN acquires the shared lock; after a reset
 * BEGIN with the same signed manifest resumes reception. FINISH verifies staging
 * only. The caller may abort on disconnect or a bounded idle timeout. */
tp_result tp_stage_session_execute(void *,const tp_upgrade_commands *,tp_upgrade_command,
    const uint8_t *,size_t,tp_upgrade_reply *);
/* Explicit install starts only after verified reception. Maintenance may return
 * BUSY or LOCK_BUSY while outputs are made safe; the session remains locked for retry. */
tp_result tp_stage_session_install_begin(tp_stage_session *);
tp_result tp_stage_session_install_step(tp_stage_session *);
/* Abort releases ownership, preserving durable progress for later resumption. */
void tp_stage_session_abort(tp_stage_session *);
#endif
