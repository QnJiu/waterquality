#include "tp_preflight.h"
#include <assert.h>
#include <math.h>
typedef struct { bool ok; float temp; tp_result result; uint16_t faults; } fake;
static bool interlock(void *p) { return ((fake *)p)->ok; }
static tp_result temperature(void *p,float *out) { fake *f=p; *out=f->temp; return f->result; }
static uint16_t resources(void *p) { return ((fake *)p)->faults; }
int main(void) {
    fake f={true,20,TP_OK,0}; tp_preflight_io io={&f,interlock,temperature,resources,80}; uint16_t mask=123;
    assert(tp_preflight_check(&io,&mask)==TP_OK && mask==0);
    f.ok=false; f.temp=80; f.faults=TP_PREFLIGHT_REAGENT|TP_PREFLIGHT_LEVEL;
    assert(tp_preflight_check(&io,&mask)==TP_IO_ERROR && mask==(TP_PREFLIGHT_INTERLOCK|TP_PREFLIGHT_OVERTEMPERATURE|TP_PREFLIGHT_REAGENT|TP_PREFLIGHT_LEVEL));
    f.ok=true; f.temp=NAN; f.faults=0x4000;
    assert(tp_preflight_check(&io,&mask)==TP_IO_ERROR && mask==(TP_PREFLIGHT_TEMPERATURE_SENSOR|TP_PREFLIGHT_RESOURCE_UNKNOWN));
    f.result=TP_IO_ERROR; f.temp=20; f.faults=0;
    assert(tp_preflight_check(&io,&mask)==TP_IO_ERROR && mask==TP_PREFLIGHT_TEMPERATURE_SENSOR);
    io.resource_faults=0; assert(tp_preflight_check(&io,&mask)==TP_INVALID);
    return 0;
}
