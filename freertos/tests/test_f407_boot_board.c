#include "tp_f407_boot_board.h"
#include "tp_f407_boot_identity.h"
#include "tp_f407_boot_flash.h"
#include "tp_f407_i2c.h"
#include <assert.h>
#include <string.h>
const uint32_t tp_f407_boot_product_id=123;
const uint8_t tp_f407_boot_public_key[64]={1,2,3};
static unsigned sequence, irq_order, safe_order, clock_order, i2c_order;
static tp_result init_result=TP_OK, shutdown_result=TP_OK;
void tp_test_disable_irq(void) { irq_order=++sequence; }
void tp_f407_emergency_off(void) { safe_order=++sequence; }
void SystemCoreClockUpdate(void) { clock_order=++sequence; }
tp_result tp_f407_i2c_init(tp_f407_i2c *bus) { i2c_order=++sequence; bus->initialized=init_result==TP_OK; return init_result; }
tp_result tp_f407_i2c_poll_shutdown(tp_f407_i2c *bus) { assert(bus->initialized); return shutdown_result; }
static tp_result write_fake(void *x,uint8_t d,const uint8_t *p,size_t n) { (void)x;(void)d;(void)p;(void)n; return TP_OK; }
static tp_result read_fake(void *x,uint8_t d,const uint8_t *p,size_t n,uint8_t *v,size_t m) { (void)x;(void)d;(void)p;(void)n;(void)v;(void)m; return TP_OK; }
tp_i2c_io tp_f407_i2c_io(tp_f407_i2c *bus) { return (tp_i2c_io){bus,write_fake,read_fake}; }
int main(void) {
    tp_f407_boot_options o;
    assert(tp_f407_boot_board_options(NULL)==TP_INVALID);
    assert(tp_f407_boot_board_options(&o)==TP_OK);
    assert(irq_order<safe_order && safe_order<clock_order && clock_order<i2c_order);
    assert(o.product_id==123 && !memcmp(o.public_key,tp_f407_boot_public_key,64));
    assert(o.fram_bus.ctx && o.fram_bus.write && o.fram_bus.write_read);
    assert(o.image_ctx && o.image_read==tp_f407_boot_flash_read);
    tp_f407_boot_flash *f=o.image_ctx;
    assert((uintptr_t)f->slot[0]==TP_F407_IMAGE0_BASE && (uintptr_t)f->slot[1]==TP_F407_IMAGE1_BASE);
    assert(tp_f407_boot_board_quiesce()==TP_OK);
    shutdown_result=TP_TIMEOUT; assert(tp_f407_boot_board_quiesce()==TP_TIMEOUT);
    memset(&o,0xff,sizeof(o)); init_result=TP_IO_ERROR;
    assert(tp_f407_boot_board_options(&o)==TP_IO_ERROR);
    assert(!o.product_id && !o.image_read && !o.fram_bus.write);
    return 0;
}
