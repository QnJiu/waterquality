#include "tp_cpu_usage.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tp_counter_extension counter={0};
    assert(tp_counter_extend(&counter,0xfffffff0u)==UINT64_C(0xfffffff0));
    assert(tp_counter_extend(&counter,0x10u)==UINT64_C(0x100000010));
    assert(tp_counter_extend(&counter,0x20u)==UINT64_C(0x100000020));
    assert(tp_cpu_busy_percent(1000,1000)==0);
    assert(tp_cpu_busy_percent(1000,250)==75);
    assert(tp_cpu_busy_percent(1000,0)==100);
    assert(tp_cpu_busy_percent(0,0)==UINT32_MAX);
    assert(tp_cpu_busy_percent(1000,1001)==UINT32_MAX);
    assert(tp_cpu_busy_percent(UINT64_MAX,0)==UINT32_MAX);
    return 0;
}
