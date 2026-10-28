#include "tp_f030_status_output.h"
#include "tp_f407_status_input.h"
#include "tp_core.h"
#include "stm32f4xx.h"
#include <assert.h>
RCC_TypeDef fake_rcc; GPIO_TypeDef fake_gpio_b, fake_gpio_c;
typedef struct { tp_sub_inputs inputs; bool fail; unsigned safe; } fixture;
static tp_result inputs(void *ctx,tp_sub_inputs *out) { fixture *f=ctx; if(f->fail) return TP_IO_ERROR; *out=f->inputs; return TP_OK; }
static tp_result send(void *c,const uint8_t *b,size_t n) { (void)c; (void)b; (void)n; return TP_OK; }
static bool interlocks(void *c) { (void)c; return tp_f407_status_input_ok(); }
static void safe(void *c) { ((fixture *)c)->safe++; }
static tp_result valves(void *c,uint32_t v) { (void)c; (void)v; return TP_OK; }
static tp_result pump(void *c,int32_t s,uint32_t h) { (void)c; (void)s; (void)h; return TP_OK; }
static tp_result poll(void *c,bool *done) { (void)c; *done=false; return TP_OK; }
static tp_result temp(void *c,float *v) { (void)c; *v=20; return TP_OK; }
static tp_result heater(void *c,float v) { (void)c; (void)v; return TP_OK; }
static tp_result adc(void *c,uint8_t ch,float *v) { (void)c; (void)ch; *v=1; return TP_OK; }
/* Model the wire after the F030 output register is written. */
static void wire(void) { fake_gpio_c.IDR=(fake_gpio_b.BSRR==1u) ? 1u<<12 : 0; }
int main(void) {
    fixture f={0}; tp_sub_node node; tp_sub_node_io io={&f,send,inputs};
    tp_hal hal={&f,valves,pump,poll,temp,heater,adc,interlocks,safe};
    tp_controller controller; const tp_step recipe[]={
        {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={10,100}}, {.kind=TP_STEP_END}};
    tp_init(&controller,&hal,NULL,NULL,80);
    tp_f030_status_output_init(); tp_f407_status_input_init(); wire();
    assert(tp_start(&controller,recipe,2,0)==TP_IO_ERROR && controller.fault==TP_FAULT_INTERLOCK);
    assert(tp_sub_node_init(&node,&io,115200,10)==TP_OK);
    tp_f030_status_output_update(&node); wire();
    assert(tp_clear_fault(&controller)==TP_OK && tp_start(&controller,recipe,2,1)==TP_OK);
    tp_tick(&controller,2); assert(controller.state==TP_RUNNING);
    f.inputs.leak_fault=true; tp_f030_status_output_update(&node); wire(); tp_tick(&controller,3);
    assert(controller.state==TP_FAILED && controller.fault==TP_FAULT_INTERLOCK && f.safe>=2);
    f.inputs.leak_fault=false; f.fail=true; tp_f030_status_output_update(&node); wire();
    assert(tp_clear_fault(&controller)==TP_INVALID);
    f.fail=false; tp_f030_status_output_update(&node); wire();
    assert(tp_clear_fault(&controller)==TP_OK);
    return 0;
}
