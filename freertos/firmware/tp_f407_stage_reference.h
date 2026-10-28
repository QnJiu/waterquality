#ifndef TP_F407_STAGE_REFERENCE_H
#define TP_F407_STAGE_REFERENCE_H
#include "tp_f407_stage_service.h"
#include "tp_f407_stage_uart.h"
#include "tp_f407_w25q128_spi.h"
/* Optional reference assembly. Zero-initialize and keep at a fixed address.
 * The application owns all W25Q128 storage. On any failure halt and reset;
 * earlier initialization may have created tasks or allocated resources. */
typedef struct {
    tp_f407_w25q128_spi spi;
    tp_w25q128 flash;
    tp_w25q128_image image;
    tp_f407_stage_uart uart;
    tp_f407_stage_service service;
    bool attempted;
} tp_f407_stage_reference;
/* Call after upgrade service startup, before scheduler. Two 384 KiB staging
 * partitions begin at 0 and 0x60000; they are not executable slots. options
 * supplies timing, stack and priority. finished executes in the stage task. */
tp_result tp_f407_stage_reference_start(tp_f407_stage_reference *,tp_f407_upgrade_service *,
    const tp_f407_stage_options *,void *finished_ctx,void (*finished)(void *,tp_result));
#endif
