#include "tp_f407_boot_board.h"
#include "tp_f407_emergency.h"
#include "stm32f4xx.h"

/* The build embeds a signed manifest; the image must already be in its slot. */
extern const uint8_t tp_factory_manifest[TP_SIGNED_MANIFEST_SIZE];
volatile tp_result tp_factory_result=TP_BUSY;
static tp_f407_boot boot;
static tp_f407_boot_options options;
static void halt(void) { __disable_irq(); tp_f407_emergency_off(); for (;;) __NOP(); }
int main(void) {
    tp_factory_result=tp_f407_boot_board_options(&options);
    if (tp_factory_result!=TP_OK) halt();
    tp_factory_result=tp_f407_boot_init(&boot,&options);
    if (tp_factory_result!=TP_OK) halt();
    tp_factory_result=tp_f407_boot_factory_provision(&boot,tp_factory_manifest,sizeof(tp_factory_manifest));
    halt();
}
