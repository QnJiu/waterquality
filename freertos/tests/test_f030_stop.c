#include <assert.h>
#include <stdlib.h>
#include "stm32f0xx.h"
#include "tp_f030_rtc.h"
#include "tp_f030_subordinate.h"
#include "tp_f030_wake_input.h"
RTC_TypeDef rtc; EXTI_TypeDef exti; NVIC_Type nvic; SCB_Type scb;
SysTick_Type systick; TIM_TypeDef tim; PWR_TypeDef pwr;
uint32_t SystemCoreClock=8000000u;
static bool irq_masked, wake_active, abort_sleep, arm_ok=true, read_ok=true, init_ok=true;
static bool pending_after_arm, pending_after_read, wake_with_pending;
static unsigned wfi_calls, arm_calls, ack_calls, reads;
static uint32_t stepped, added_us, elapsed_counts=3;
void fake_disable(void) { assert(!irq_masked); irq_masked=true; }
void fake_enable(void) { assert(irq_masked); irq_masked=false; }
void fake_wfi(void) {
    assert(irq_masked && (scb.SCR & SCB_SCR_SLEEPDEEP_Msk));
    assert(!(tim.CR1 & TIM_CR1_CEN) && !(systick.CTRL & SysTick_CTRL_ENABLE_Msk));
    wfi_calls++;
    if(wake_with_pending) nvic.ISPR[0]|=1u<<7;
}
void tp_platform_assert(const char *file,int line) { (void)file;(void)line; abort(); }
eSleepModeStatus eTaskConfirmSleepModeStatus(void) { assert(irq_masked); return abort_sleep ? eAbortSleep : eStandardSleep; }
void vTaskStepTick(TickType_t n) { assert(irq_masked); stepped+=n; }
bool tp_f030_wake_input_active(void) { return wake_active; }
void tp_f030_subordinate_add_sleep_us(uint32_t n) { assert(irq_masked && !(tim.CR1 & TIM_CR1_CEN)); added_us+=n; }
bool tp_f030_rtc_init(uint32_t n) { assert(n); return init_ok; }
bool tp_f030_rtc_arm(uint32_t ms,uint32_t n,tp_f030_rtc_stamp *s) {
    assert(irq_masked && n && ms==6); arm_calls++;
    *s=(tp_f030_rtc_stamp){10,1000};
    if(pending_after_arm) nvic.ISPR[0]|=1u<<7;
    return arm_ok;
}
bool tp_f030_rtc_read(tp_f030_rtc_stamp *s,uint32_t attempts) {
    assert(irq_masked && attempts==8); reads++;
    if(!read_ok) return false;
    if(pending_after_read && reads%2) nvic.ISPR[0]|=1u<<7;
    *s=(tp_f030_rtc_stamp){10,(uint16_t)(reads%2 ? 1000 : 1000-elapsed_counts)};
    return true;
}
bool tp_f030_rtc_ack_alarm(uint32_t n) { assert(irq_masked && n); ack_calls++; rtc.CR=0; rtc.ISR=0; exti.PR=0; return true; }
#include "../firmware_f030/tp_f030_stop.c"
int main(void) {
    exti.IMR=1u<<19; assert(!tp_f030_stop_init()); exti.IMR=0;
    assert(tp_f030_stop_init());
    assert(!tp_f030_stop_init());
    tim.CR1=TIM_CR1_CEN; systick.CTRL=7; systick.VAL=4000; nvic.ISER[0]|=1u<<7;
    wake_active=true; vPortSuppressTicksAndSleep(10); assert(!arm_calls && !wfi_calls && !irq_masked);
    wake_active=false; abort_sleep=true; vPortSuppressTicksAndSleep(10); assert(!arm_calls);
    abort_sleep=false; nvic.ISPR[0]|=1u<<7; vPortSuppressTicksAndSleep(10); assert(!arm_calls);
    nvic.ISPR[0]=0; pending_after_arm=true; vPortSuppressTicksAndSleep(10);
    assert(arm_calls==1 && ack_calls==1 && !wfi_calls && (nvic.ISPR[0]&(1u<<7)));
    pending_after_arm=false; nvic.ISPR[0]=0; arm_ok=false; vPortSuppressTicksAndSleep(10);
    assert(arm_calls==2 && ack_calls==2 && !wfi_calls);
    arm_ok=true; wake_with_pending=true; vPortSuppressTicksAndSleep(10);
    assert(wfi_calls==1 && ack_calls==3 && reads==2 && stepped==3 && added_us==2929);
    assert((tim.CR1 & TIM_CR1_CEN) && systick.CTRL==7 && systick.LOAD==7999 && !irq_masked);
    assert((nvic.ISPR[0]&(1u<<7)) && !(scb.SCR & SCB_SCR_SLEEPDEEP_Msk));
    nvic.ISPR[0]=0; pending_after_read=true; vPortSuppressTicksAndSleep(10);
    assert(wfi_calls==1 && ack_calls==4 && reads==4 && added_us==5859);
    assert(nvic.ISPR[0] & (1u<<7));
    nvic.ISPR[0]=0; pending_after_read=false; read_ok=false; vPortSuppressTicksAndSleep(10);
    assert(wfi_calls==1 && ack_calls==5 && (tim.CR1 & TIM_CR1_CEN) && systick.CTRL==7 && !irq_masked);
    return 0;
}
