#include <stdint.h>
#include <assert.h>
#include <string.h>
static int irq_disabled, outputs_off, resets, calls;
static void test_disable_irq(void) { irq_disabled++; }
static void test_reset(void) { assert(irq_disabled && outputs_off); resets++; }
#define tskIDLE_PRIORITY 0
#define __disable_irq test_disable_irq
#define NVIC_SystemReset test_reset
#include "../firmware/tp_f407_teaching_ota.c"

const uint32_t tp_f407_boot_product_id=42;
const uint8_t tp_f407_boot_public_key[64]={1,2};
void tp_f407_emergency_off(void) { assert(irq_disabled); outputs_off++; }
static tp_result bus_result=TP_OK, flash_result=TP_OK, start_result=TP_OK;
tp_result tp_f407_i2c_init_on(tp_f407_i2c *bus,unsigned peripheral) {
    assert(bus==&upgrade_bus && peripheral==2 && calls++==0); return bus_result;
}
tp_i2c_io tp_f407_i2c_io(tp_f407_i2c *bus) {
    assert(bus==&upgrade_bus); tp_i2c_io io={0}; io.ctx=bus; return io;
}
tp_result tp_f407_flash_controller_init(tp_f407_flash_controller *controller,uint32_t limit,tp_f407_flash_ops *ops) {
    assert(controller==&flash && limit==1000000 && calls++==1); ops->ctx=controller; return flash_result;
}
tp_result tp_f407_ota_reference_start(void *ctx,tp_f407_app *app) {
    assert(ctx==&ota && app && calls++==2);
    assert(ota.upgrade.options.storage.fram_bus.ctx==&upgrade_bus);
    assert(ota.upgrade.options.storage.flash_ops.ctx==&flash);
    return start_result;
}
tp_result tp_f407_upgrade_uart_on_finish(tp_f407_upgrade_uart *u,void *ctx,void (*done)(void *)) {
    assert(u==&ota.upgrade.service.uart && !ctx && done && calls++==3);
    done(ctx); return TP_OK;
}
int main(void) {
    tp_f407_app_options options={0}; tp_f407_app app={0};
    assert(tp_f407_teaching_ota_configure(NULL)==TP_INVALID);
    assert(tp_f407_teaching_ota_configure(&options)==TP_OK);
    assert(options.business_i2c==3 && options.start_services);
    assert(ota.upgrade.options.storage.product_id==42);
    assert(memcmp(ota.upgrade.options.storage.public_key,tp_f407_boot_public_key,64)==0);
    bus_result=TP_IO_ERROR; assert(options.start_services(NULL,&app)==TP_IO_ERROR && calls==1);
    calls=0; bus_result=TP_OK; flash_result=TP_TIMEOUT;
    assert(options.start_services(NULL,&app)==TP_TIMEOUT && calls==2);
    calls=0; flash_result=TP_OK; start_result=TP_IO_ERROR;
    assert(options.start_services(NULL,&app)==TP_IO_ERROR && calls==3);
    calls=0; start_result=TP_OK; assert(options.start_services(NULL,&app)==TP_OK && calls==4);
    assert(resets==1 && outputs_off==1);
    ota.upgrade.options.finished(NULL,TP_OK); assert(resets==1);
    ota.upgrade.options.finished(NULL,TP_IO_ERROR); assert(resets==2 && outputs_off==2);
    ota.stage_finished(NULL,TP_TIMEOUT); assert(resets==3 && outputs_off==3);
    return 0;
}
