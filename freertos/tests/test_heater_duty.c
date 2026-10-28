#include "tp_heater_duty.h"
#include <assert.h>
#include <math.h>
int main(void) {
    uint32_t t=77;
    assert(tp_heater_duty_ticks(0,1000000,&t)==TP_OK && t==0);
    assert(tp_heater_duty_ticks(.5f,1000000,&t)==TP_OK && t==500000);
    assert(tp_heater_duty_ticks(1,1000000,&t)==TP_OK && t==1000000);
    assert(tp_heater_duty_ticks(NAN,1000000,&t)==TP_INVALID && t==1000000);
    assert(tp_heater_duty_ticks(-.1f,1000000,&t)==TP_INVALID);
    assert(tp_heater_duty_ticks(1.1f,1000000,&t)==TP_INVALID);
    assert(tp_heater_duty_ticks(1,0,&t)==TP_INVALID);
    return 0;
}
