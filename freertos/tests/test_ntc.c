#include "tp_ntc.h"
#include <assert.h>
#include <math.h>
int main(void) {
    float c=999;
    assert(tp_ntc_10k_3950_celsius(2048,&c)==TP_OK && fabsf(c-25.0f)<0.1f);
    assert(tp_ntc_10k_3950_celsius(1000,&c)==TP_OK && c>50 && c<65);
    assert(tp_ntc_10k_3950_celsius(3000,&c)==TP_OK && c<10 && c>0);
    c=999; assert(tp_ntc_10k_3950_celsius(0,&c)==TP_INVALID && c==999);
    assert(tp_ntc_10k_3950_celsius(4095,&c)==TP_INVALID);
    assert(tp_ntc_10k_3950_celsius(2048,NULL)==TP_INVALID);
    return 0;
}
