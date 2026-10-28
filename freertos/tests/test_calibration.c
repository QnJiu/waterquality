#include "tp_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    tp_calibration base={1,0,0,0.001f,10}, fitted={0};
    tp_calibration_point zero={1,1,0,0}, span={1,0.1f,0,5};
    assert(tp_calibrate_two_point(&base,&zero,&span,0.01f,&fitted)==TP_OK);
    assert(fabsf(fitted.slope-5)<1e-5f && fabsf(fitted.intercept)<1e-5f);
    tp_calibration_point ref={1,0.31622777f,0,2.5f}; float actual=99; bool passed=false;
    assert(tp_calibration_check(&fitted,&ref,0.001f,&actual,&passed)==TP_OK && passed);
    ref.concentration=3; assert(tp_calibration_check(&fitted,&ref,0.1f,&actual,&passed)==TP_OK && !passed);
    tp_calibration untouched=fitted; span.reacted=1;
    assert(tp_calibrate_two_point(&base,&zero,&span,0.01f,&fitted)==TP_INVALID);
    assert(fitted.slope==untouched.slope && fitted.intercept==untouched.intercept);
    span.reacted=0.1f; span.concentration=11;
    assert(tp_calibrate_two_point(&base,&zero,&span,0.01f,&fitted)==TP_INVALID);
    span.concentration=5; span.blank=NAN;
    assert(tp_calibrate_two_point(&base,&zero,&span,0.01f,&fitted)==TP_INVALID);
    actual=99; passed=true; ref.reacted=0;
    assert(tp_calibration_check(&fitted,&ref,0.1f,&actual,&passed)==TP_INVALID && actual==99 && passed);
    assert(tp_concentration(&base,1e30f,1e-20f,0,&actual)==TP_INVALID); /* below min signal */
    base.min_signal=1e-30f; base.max_concentration=100;
    assert(tp_concentration(&base,1e30f,1e-20f,0,&actual)==TP_OK && fabsf(actual-50)<0.001f);
    puts("calibration checks passed"); return 0;
}
