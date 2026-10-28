#include "tp_f407_board_config.h"
#include "tp_f407_ota_reference.h"
#include "tp_f407_flash_controller.h"
#include "tp_f407_emergency.h"
#include "../firmware_boot/tp_f407_boot_identity.h"
#include "stm32f4xx.h"
#include <string.h>

static tp_f407_i2c upgrade_bus;
static tp_f407_flash_controller flash;
static tp_f407_ota_reference ota;

/* The application health gate separately checks its configured interlocks.
 * This reference board has no additional local health inputs. */
static bool board_healthy(void *ctx) { (void)ctx; return true; }
static void safe_reset(void) {
    __disable_irq();
    tp_f407_emergency_off();
    NVIC_SystemReset();
}
static void trial_finished(void *ctx,tp_result result) {
    (void)ctx;
    if(result!=TP_OK) safe_reset();
}
static void install_finished(void *ctx,tp_result result) {
    (void)ctx; (void)result;
    safe_reset();
}
static void wired_finished(void *ctx) { (void)ctx; safe_reset(); }
static tp_result start_ota(void *ctx,tp_f407_app *app) {
    (void)ctx;
    tp_result r=tp_f407_i2c_init_on(&upgrade_bus,2);
    if(r!=TP_OK) return r;
    ota.upgrade.options.storage.fram_bus=tp_f407_i2c_io(&upgrade_bus);
    r=tp_f407_flash_controller_init(&flash,1000000,&ota.upgrade.options.storage.flash_ops);
    if(r!=TP_OK) return r;
    r=tp_f407_ota_reference_start(&ota,app);
    if(r!=TP_OK) return r;
    return tp_f407_upgrade_uart_on_finish(&ota.upgrade.service.uart,NULL,wired_finished);
}
tp_result tp_f407_teaching_ota_configure(tp_f407_app_options *options) {
    if(!options || !tp_f407_boot_product_id) return TP_INVALID;
    bool nonzero=false;
    for(unsigned i=0;i<64;i++) nonzero |= tp_f407_boot_public_key[i]!=0;
    if(!nonzero) return TP_INVALID;
    options->business_i2c=3; /* separate from I2C2 upgrade FRAM */
    options->start_services=start_ota;
    ota.upgrade.options.storage.product_id=tp_f407_boot_product_id;
    memcpy(ota.upgrade.options.storage.public_key,tp_f407_boot_public_key,64);
    ota.upgrade.options.healthy=board_healthy;
    ota.upgrade.options.finished=trial_finished;
    ota.upgrade.options.frame_timeout_ms=1000;
    ota.upgrade.options.stable_ms=5000;
    ota.upgrade.options.max_gap_ms=1000;
    ota.upgrade.options.period_ms=500;
    ota.upgrade.options.trial_timeout_ms=30000;
    /* Direct UART upgrades verify signed manifests in this task. */
    ota.upgrade.options.uart_stack_words=2048;
    ota.upgrade.options.trial_stack_words=1024;
    ota.upgrade.options.uart_priority=tskIDLE_PRIORITY+1;
    ota.upgrade.options.trial_priority=tskIDLE_PRIORITY+1;
    ota.stage_options.frame_timeout_ms=1000;
    ota.stage_options.idle_timeout_ms=60000;
    ota.stage_options.period_ms=5;
    ota.stage_options.stack_words=2048;
    ota.stage_options.priority=tskIDLE_PRIORITY+1;
    ota.stage_finished=install_finished;
    return TP_OK;
}
