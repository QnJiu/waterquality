#include "tp_f407_stage_reference.h"
#include "tp_freertos_flash_clock.h"
/* Conservative software timeouts; verify against the selected flash device. */
#define PROGRAM_TIMEOUT_MS 100u
#define ERASE_TIMEOUT_MS 3000u
#define SLOT_SIZE TP_F407_IMAGE_BANK_SIZE

tp_result tp_f407_stage_reference_start(tp_f407_stage_reference *r,tp_f407_upgrade_service *upgrade,
    const tp_f407_stage_options *settings,void *ctx,void (*done)(void *,tp_result)) {
    if(!r || !upgrade || !settings || !done || r->attempted ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    r->attempted=true;
    tp_result result=tp_f407_w25q128_spi_init(&r->spi);
    if(result!=TP_OK) return result;
    tp_w25q128_io io=tp_f407_w25q128_spi_io(&r->spi);
    result=tp_w25q128_init(&r->flash,&io);
    if(result!=TP_OK) return result;
    tp_f407_stage_options options=*settings;
    result=tp_f407_stage_storage_init(&options,upgrade,&r->image,&r->flash,NULL,
        tp_freertos_flash_now_ms,tp_freertos_flash_sleep_ms,0,SLOT_SIZE,SLOT_SIZE,
        PROGRAM_TIMEOUT_MS,ERASE_TIMEOUT_MS);
    if(result!=TP_OK) return result;
    result=tp_f407_stage_uart_init(&r->uart,&options.transport,ctx,done);
    if(result!=TP_OK) return result;
    return tp_f407_stage_service_start(&r->service,upgrade,&options);
}
