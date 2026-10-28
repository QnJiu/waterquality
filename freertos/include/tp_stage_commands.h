#ifndef TP_STAGE_COMMANDS_H
#define TP_STAGE_COMMANDS_H
#include "tp_stage_receiver.h"
#include "tp_upgrade_commands.h"
/* One task owns this endpoint and staging storage. Hold the shared upgrade lock
 * for the entire reception session; see bootloader/README.md for handoff. */
typedef struct {
    tp_stage_receiver receiver;
    tp_boot_io boot;
    tp_download_io storage;
    uint32_t capacity[2], product_id;
    uint8_t public_key[64];
    bool initialized, opened;
} tp_stage_commands;
tp_result tp_stage_commands_init(tp_stage_commands *,const tp_boot_io *,
    const tp_download_io *,const uint32_t capacity[2],const uint8_t public_key[64],uint32_t product_id);
/* Compatible with tp_upgrade_wire_set_executor. BEGIN carries the signed
 * manifest and must be repeated after reset. FINISH verifies staging only;
 * it does not install the image or change the boot slot. */
tp_result tp_stage_commands_execute(void *,const tp_upgrade_commands *,tp_upgrade_command,
    const uint8_t *,size_t,tp_upgrade_reply *);
#endif
