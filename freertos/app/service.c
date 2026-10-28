#include "tp_service.h"
#include <string.h>
/* Holding register addresses are zero-based. See docs/MODBUS.md. */
static uint32_t join(uint16_t hi, uint16_t lo) { return ((uint32_t)hi << 16) | lo; }
static bool calibration_running(const tp_service *s) {
    return s->cal_attached && (s->auto_cal.state==TP_AUTO_CAL_LOW || s->auto_cal.state==TP_AUTO_CAL_HIGH);
}
static bool standard_running(const tp_service *s) {
    return s->standard_attached && s->auto_standard.state==TP_STANDARD_RUNNING;
}
static tp_result run_preflight(void *ctx) {
    tp_service *s=ctx;
    if (!s->preflight) return TP_OK;
    uint16_t faults=TP_PREFLIGHT_NOT_RUN;
    tp_result r=s->preflight(s->preflight_ctx,&faults);
    s->preflight_faults=faults;
    if(r!=TP_OK || faults) {
        if(!faults) s->preflight_faults=TP_PREFLIGHT_NOT_RUN;
        s->controller->hal.safe_outputs(s->controller->hal.ctx);
        return TP_IO_ERROR;
    }
    return TP_OK;
}
static tp_result start(tp_service *s) {
    if (s->maintenance || s->save_status==2 || calibration_running(s) || standard_running(s) ||
        (s->clock_status && s->clock_status(s->clock_ctx)==2)) {
        s->last_start_result=TP_BUSY; return TP_BUSY;
    }
    if (s->controller->state!=TP_RUNNING && s->controller->state!=TP_FAILED && run_preflight(s)!=TP_OK) {
        s->last_start_result=TP_IO_ERROR; return TP_IO_ERROR;
    }
    s->last_start_result=tp_start(s->controller,s->recipe,s->recipe_len,s->now_ms);
    if (s->last_start_result==TP_OK) s->controller->single_step=s->schedule.mode==TP_MODE_CONTROLLED;
    return s->last_start_result;
}
tp_result tp_service_init(tp_service *s, tp_controller *controller,
                          const tp_step *recipe, size_t len, uint32_t now) {
    if (!s || !controller || !recipe || !len) return TP_INVALID;
    memset(s, 0, sizeof(*s)); s->controller = controller; s->recipe = recipe; s->recipe_len = len;
    s->now_ms = now; s->last_start_result = TP_OK; s->preflight_faults=TP_PREFLIGHT_NOT_RUN;
    return tp_schedule_configure(&s->schedule, TP_MODE_CONTROLLED, 0, 0, 0, now, false);
}
tp_result tp_service_attach_clock(tp_service *s,void *ctx,tp_result (*set)(void *,uint32_t),uint8_t (*status)(void *)) {
    if (!s || !s->controller || !set || !status || s->set_clock || s->controller->state==TP_RUNNING) return TP_INVALID;
    s->clock_ctx=ctx; s->set_clock=set; s->clock_status=status; return TP_OK;
}
tp_result tp_service_attach_auto_calibration(tp_service *s,
    const tp_step *low, size_t low_len, const tp_step *high, size_t high_len,
    float low_concentration, float high_concentration, float min_delta) {
    if (!s || !s->controller || s->cal_attached || s->controller->state==TP_RUNNING) return TP_INVALID;
    tp_auto_calibration candidate;
    tp_result r=tp_auto_cal_init(&candidate,s->controller,low,low_len,high,high_len,
                                 low_concentration,high_concentration,min_delta,s,run_preflight);
    if (r!=TP_OK) return r;
    s->auto_cal=candidate; s->cal_recipes[0]=low; s->cal_recipes[1]=high;
    s->cal_lengths[0]=low_len; s->cal_lengths[1]=high_len;
    s->cal_concentrations[0]=low_concentration; s->cal_concentrations[1]=high_concentration;
    s->cal_min_delta=min_delta; s->cal_attached=true; return TP_OK;
}
static tp_result start_auto_cal(tp_service *s) {
    if (!s->cal_attached || !s->request_save) return TP_INVALID;
    if (s->maintenance || s->save_status==2 || calibration_running(s) || standard_running(s) ||
        (s->clock_status && s->clock_status(s->clock_ctx)==2) ||
        s->controller->state==TP_RUNNING || s->controller->state==TP_FAILED) return TP_BUSY;
    tp_auto_calibration candidate;
    tp_result r=tp_auto_cal_init(&candidate,s->controller,s->cal_recipes[0],s->cal_lengths[0],
        s->cal_recipes[1],s->cal_lengths[1],s->cal_concentrations[0],s->cal_concentrations[1],
        s->cal_min_delta,s,run_preflight);
    if (r!=TP_OK) return r;
    r=tp_auto_cal_start(&candidate,s->now_ms);
    if (r!=TP_OK) { candidate.state=TP_AUTO_CAL_FAILED; candidate.error=r; }
    s->auto_cal=candidate;
    return r;
}
tp_result tp_service_attach_auto_standard(tp_service *s, const tp_step *recipe,
    size_t length, float concentration, float tolerance) {
    if (!s || !s->controller || s->standard_attached || s->controller->state==TP_RUNNING)
        return TP_INVALID;
    tp_auto_standard candidate;
    tp_result r=tp_auto_standard_init(&candidate,s->controller,recipe,length,concentration,
                                       tolerance,s,run_preflight);
    if (r!=TP_OK) return r;
    s->auto_standard=candidate; s->standard_recipe=recipe; s->standard_length=length;
    s->standard_concentration=concentration; s->standard_tolerance=tolerance;
    s->standard_attached=true; return TP_OK;
}
static tp_result start_auto_standard(tp_service *s) {
    if (!s->standard_attached) return TP_INVALID;
    if (s->maintenance || s->save_status==2 || calibration_running(s) || standard_running(s) ||
        (s->clock_status && s->clock_status(s->clock_ctx)==2) ||
        s->controller->state==TP_RUNNING || s->controller->state==TP_FAILED) return TP_BUSY;
    tp_auto_standard candidate;
    tp_result r=tp_auto_standard_init(&candidate,s->controller,s->standard_recipe,s->standard_length,
        s->standard_concentration,s->standard_tolerance,s,run_preflight);
    if (r!=TP_OK) return r;
    r=tp_auto_standard_start(&candidate,s->now_ms);
    if (r!=TP_OK) { candidate.state=TP_STANDARD_FAILED; candidate.error=r; }
    s->auto_standard=candidate; return r;
}
tp_result tp_service_attach_preflight(tp_service *s,void *ctx,tp_result (*check)(void *,uint16_t *)) {
    if(!s || !s->controller || !s->controller->hal.safe_outputs || !check || s->preflight ||
       s->controller->state==TP_RUNNING) return TP_INVALID;
    s->preflight_ctx=ctx; s->preflight=check; s->preflight_faults=TP_PREFLIGHT_NOT_RUN; return TP_OK;
}
tp_result tp_service_attach_adc_recovery(tp_service *s,void *ctx,tp_result (*recover)(void *)) {
    if (!s || !s->controller || !s->controller->hal.safe_outputs || !recover ||
        s->recover_adc || s->controller->state==TP_RUNNING) return TP_INVALID;
    s->recover_ctx=ctx; s->recover_adc=recover; return TP_OK;
}
static void apply_config(tp_service *s, const tp_config *cfg) {
    s->standard_check_status=0; s->standard_check_actual=0;
    if (s->standard_attached && s->auto_standard.state==TP_STANDARD_COMPLETE)
        s->auto_standard.state=TP_STANDARD_IDLE;
    if (s->schedule.mode != (tp_mode)cfg->mode ||
        s->schedule.period_ms != cfg->period_seconds * 1000u ||
        s->schedule.daily_minute != cfg->daily_minute ||
        s->schedule.utc_offset_minutes != cfg->utc_offset_minutes) {
        (void)tp_schedule_configure(&s->schedule, (tp_mode)cfg->mode,
            cfg->period_seconds * 1000u, cfg->daily_minute,
            cfg->utc_offset_minutes, s->now_ms, false);
    }
    if (s->cal_attached && s->auto_cal.state==TP_AUTO_CAL_COMPLETE)
        s->auto_cal.state=TP_AUTO_CAL_IDLE; /* configuration changed; discard stale fit */
    s->config = *cfg; s->controller->calibration = cfg->calibration;
    s->controller->pid.kp = cfg->kp; s->controller->pid.ki = cfg->ki;
    s->controller->pid.kd = cfg->kd; s->controller->pid.deadband_c = cfg->deadband_c;
    s->controller->pid.output_max = cfg->output_max; s->controller->pid.integral_max = cfg->integral_max;
    s->controller->max_temperature_c = cfg->max_temperature_c;
}
tp_result tp_service_attach_config(tp_service *s, const tp_config *cfg,
    void *ctx, tp_result (*request_save)(void *, const tp_config *)) {
    if (!s || !s->controller || !request_save || s->controller->state == TP_RUNNING ||
        tp_config_validate(cfg) != TP_OK) return TP_INVALID;
    apply_config(s,cfg); s->save_ctx=ctx; s->request_save=request_save; s->save_status=1;
    return TP_OK;
}
void tp_service_save_complete(tp_service *s, tp_result result) {
    if (s && s->save_status == 2) s->save_status = result == TP_OK ? 3 : 4;
}
tp_result tp_service_attach_log(tp_service *s,void *ctx,tp_result (*request)(void *,bool,uint32_t)) {
    if(!s || !s->controller || !request || s->request_log) return TP_INVALID;
    s->log_ctx=ctx; s->request_log=request; s->log_status=1; return TP_OK;
}
void tp_service_log_complete(tp_service *s,tp_result result,const tp_event *event) {
    if(!s || s->log_status!=2) return;
    memset(&s->log_event,0,sizeof(s->log_event));
    if(result==TP_OK && event) { s->log_event=*event; s->log_status=3; }
    else s->log_status=result==TP_INVALID ? 4 : 5;
}
static void log_words(const tp_service *s,uint16_t *v) {
    uint64_t time; memcpy(&time,&s->log_event.utc_seconds,sizeof(time));
    v[0]=s->log_status; v[1]=(uint16_t)(s->log_event.sequence>>16); v[2]=(uint16_t)s->log_event.sequence;
    v[3]=s->log_event.type; v[4]=s->log_event.detail; v[5]=s->log_event.utc_valid ? 1u : 0u;
    for(unsigned i=0;i<4;i++) v[6+i]=(uint16_t)(time>>(48u-16u*i));
    v[10]=(uint16_t)(s->log_event.value>>16); v[11]=(uint16_t)s->log_event.value;
}static float word_float(const uint16_t *v) {
    uint32_t u=join(v[0],v[1]); float f; memcpy(&f,&u,sizeof(f)); return f;
}
static uint16_t float_word(float f, bool low) {
    uint32_t u; memcpy(&u,&f,sizeof(u)); return low ? (uint16_t)u : (uint16_t)(u>>16);
}
static void config_words(const tp_config *c, uint16_t *v) {
    const float f[]={c->calibration.slope,c->calibration.intercept,c->calibration.turbidity_factor,
        c->calibration.min_signal,c->calibration.max_concentration,c->kp,c->ki,c->kd,
        c->deadband_c,c->output_max,c->integral_max,c->max_temperature_c};
    for(unsigned i=0;i<12;i++) { v[2*i]=float_word(f[i],false); v[2*i+1]=float_word(f[i],true); }
    v[24]=(uint16_t)(c->period_seconds>>16); v[25]=(uint16_t)c->period_seconds; v[26]=c->slave_address;
}
static tp_result set_config_words(tp_service *s, const uint16_t *v) {
    if (!s->request_save || s->controller->state==TP_RUNNING || calibration_running(s) ||
        standard_running(s) || s->save_status==2 || v[26]>255)
        return TP_INVALID;
    tp_config c = s->config;
    c.calibration.slope=word_float(v); c.calibration.intercept=word_float(v+2);
    c.calibration.turbidity_factor=word_float(v+4); c.calibration.min_signal=word_float(v+6);
    c.calibration.max_concentration=word_float(v+8); c.kp=word_float(v+10);
    c.ki=word_float(v+12); c.kd=word_float(v+14); c.deadband_c=word_float(v+16);
    c.output_max=word_float(v+18); c.integral_max=word_float(v+20);
    c.max_temperature_c=word_float(v+22); c.period_seconds=join(v[24],v[25]); c.slave_address=(uint8_t)v[26];
    if (tp_config_validate(&c)!=TP_OK) return TP_INVALID;
    apply_config(s,&c); s->save_status=1; return TP_OK;
}
/* Apply both standards atomically; the normal configuration save remains explicit. */
static tp_result calibrate_words(tp_service *s, const uint16_t *v) {
    if (!s->request_save || s->controller->state==TP_RUNNING || calibration_running(s) ||
        standard_running(s) || s->save_status==2) return TP_INVALID;
    tp_calibration_point low={word_float(v),word_float(v+2),word_float(v+4),word_float(v+6)};
    tp_calibration_point high={word_float(v+8),word_float(v+10),word_float(v+12),word_float(v+14)};
    tp_config candidate=s->config;
    if (tp_calibrate_two_point(&s->config.calibration,&low,&high,word_float(v+16),
                               &candidate.calibration)!=TP_OK || tp_config_validate(&candidate)!=TP_OK)
        return TP_INVALID;
    apply_config(s,&candidate); s->save_status=1; return TP_OK;
}
static tp_result check_standard_words(tp_service *s,const uint16_t *v) {
    if (s->maintenance || s->controller->state==TP_RUNNING || calibration_running(s) ||
        standard_running(s)) return TP_BUSY;
    tp_calibration_point point={word_float(v),word_float(v+2),word_float(v+4),word_float(v+6)};
    float actual; bool passed;
    s->standard_check_status=0; s->standard_check_actual=0;
    tp_result r=tp_calibration_check(&s->controller->calibration,&point,word_float(v+8),&actual,&passed);
    if(r!=TP_OK) return r;
    s->standard_check_actual=actual; s->standard_check_status=passed ? 1 : 2;
    return TP_OK;
}
static tp_result apply_auto_cal(tp_service *s) {
    if (!s->cal_attached || !s->request_save || s->maintenance || s->save_status==2 ||
        s->controller->state==TP_RUNNING || s->controller->state==TP_FAILED ||
        calibration_running(s) || standard_running(s)) return TP_BUSY;
    tp_config candidate=s->config;
    if (tp_auto_cal_result(&s->auto_cal,&candidate.calibration)!=TP_OK ||
        tp_config_validate(&candidate)!=TP_OK) return TP_INVALID;
    apply_config(s,&candidate); s->save_status=1; return TP_OK;
}
tp_result tp_service_enter_maintenance(tp_service *s) {
    if (!s || !s->controller || !s->controller->hal.safe_outputs) return TP_INVALID;
    if (s->maintenance) return TP_OK;
    s->maintenance = true;
    if (calibration_running(s)) tp_auto_cal_cancel(&s->auto_cal);
    else if (standard_running(s)) tp_auto_standard_cancel(&s->auto_standard);
    else if (s->controller->state == TP_RUNNING) tp_stop(s->controller);
    else s->controller->hal.safe_outputs(s->controller->hal.ctx);
    return TP_OK;
}
void tp_service_tick(tp_service *s, uint32_t now, bool rtc_valid, int64_t utc) {
    if (!s || !s->controller) return;
    s->now_ms = now;
    bool clock_pending=s->clock_status && s->clock_status(s->clock_ctx)==2;
    s->clock_valid=rtc_valid && utc>=946684800LL && utc<4102444800LL && !clock_pending;
    s->clock_utc=s->clock_valid ? (uint32_t)utc : 0;
    bool busy = s->maintenance || s->controller->state == TP_RUNNING || s->controller->state == TP_FAILED ||
                clock_pending;
    if (tp_schedule_poll(&s->schedule, now, rtc_valid && !clock_pending, utc, busy)) (void)start(s);
    if (calibration_running(s)) tp_auto_cal_tick(&s->auto_cal,now);
    else if (standard_running(s)) tp_auto_standard_tick(&s->auto_standard,now);
    else tp_tick(s->controller,now);
}
static uint16_t read_one(const tp_service *s, uint16_t a) {
    uint32_t bits = 0;
    switch (a) {
    case 0: return 0; /* command is write-only */
    case 1: return (uint16_t)s->schedule.mode;
    case 2: return (uint16_t)((s->schedule.period_ms / 1000u) >> 16);
    case 3: return (uint16_t)(s->schedule.period_ms / 1000u);
    case 4: return s->schedule.daily_minute;
    case 5: return (uint16_t)s->schedule.utc_offset_minutes;
    case 10: return (uint16_t)s->controller->state;
    case 11: return (uint16_t)s->controller->fault;
    case 12: return s->controller->index > UINT16_MAX ? UINT16_MAX : (uint16_t)s->controller->index;
    case 13: return (uint16_t)s->last_start_result;
    case 17: return s->save_status;
    case 18: return s->controller->state == TP_RUNNING && s->controller->awaiting_advance;
    case 19: return s->preflight_faults;
    case 16: return s->controller->state == TP_COMPLETE && s->controller->result_valid;
    case 14: case 15:
        memcpy(&bits, &s->controller->result_mg_l, sizeof(bits));
        return a == 14 ? (uint16_t)(bits >> 16) : (uint16_t)bits;
    default: return 0;
    }
}
tp_result tp_service_attach_version(tp_service *s,void *ctx,uint32_t (*snapshot)(void *)) {
    if (!s || !s->controller || !snapshot || s->confirmed_version) return TP_INVALID;
    s->version_ctx=ctx; s->confirmed_version=snapshot; return TP_OK;
}
tp_result tp_service_attach_log_stats(tp_service *s,void *ctx,void (*snapshot)(void *,uint32_t[4])) {
    if (!s || !s->controller || !snapshot || s->log_stats) return TP_INVALID;
    s->log_stats_ctx=ctx; s->log_stats=snapshot; return TP_OK;
}
tp_result tp_service_read_holding(void *ctx, uint16_t address, uint16_t count, uint16_t *values) {
    tp_service *clock_service=ctx;
    if (clock_service && clock_service->controller && values && count && address>=122 && (uint32_t)address+count<=128) {
        uint8_t status=clock_service->clock_status ? clock_service->clock_status(clock_service->clock_ctx) : 0;
        bool valid=clock_service->clock_valid && status!=2;
        uint32_t utc=valid ? clock_service->clock_utc : 0;
        /* 123 and 124 are write-only; reading them returns zero. */
        uint16_t words[6]={status,0,0,valid ? 1u : 0u,(uint16_t)(utc>>16),(uint16_t)utc};
        for (uint16_t i=0;i<count;i++) values[i]=words[address-122+i];
        return TP_OK;
    }
    tp_service *s = ctx;
    if (s && s->controller && values && count && address>=105 && (uint32_t)address+count<=110) {
        uint16_t words[]={s->standard_attached ? (uint16_t)(s->auto_standard.state+1) : 0,
            s->standard_attached ? (uint16_t)s->auto_standard.error : 0,
            s->standard_attached && s->auto_standard.state==TP_STANDARD_COMPLETE ? (s->auto_standard.passed ? 1 : 2) : 0,
            float_word(s->auto_standard.actual,false),float_word(s->auto_standard.actual,true)};
        for(uint16_t i=0;i<count;i++) values[i]=words[address-105+i];
        return TP_OK;
    }
    if (s && s->controller && values && count && address>=102 && (uint32_t)address+count<=105) {
        uint16_t words[]={s->standard_check_status,float_word(s->standard_check_actual,false),
                          float_word(s->standard_check_actual,true)};
        for(uint16_t i=0;i<count;i++) values[i]=words[address-102+i];
        return TP_OK;
    }
    if (s && s->controller && values && count && address>=90 && (uint32_t)address+count<=92) {
        for (uint16_t i=0;i<count;i++) {
            uint16_t a=(uint16_t)(address+i);
            values[i]=a==90 ? (s->cal_attached ? (uint16_t)(s->auto_cal.state+1) : 0) :
                (s->cal_attached ? (uint16_t)s->auto_cal.error : 0);
        }
        return TP_OK;
    }
    if (s && s->controller && values && s->request_save && address>=20 &&
        (uint32_t)address+count<=47 && count) {
        uint16_t words[27]; config_words(&s->config,words);
        for (uint16_t i=0;i<count;i++) values[i]=words[address-20+i];
        return TP_OK;
    }
    if (s && s->controller && values && count && address>=119 && (uint32_t)address+count<=122) {
        uint32_t version=s->confirmed_version ? s->confirmed_version(s->version_ctx) : 0;
        uint16_t words[3]={version ? 1 : 0,(uint16_t)(version>>16),(uint16_t)version};
        for (uint16_t i=0;i<count;i++) values[i]=words[address-119+i];
        return TP_OK;
    }
    if (s && s->controller && values && count && address>=110 && (uint32_t)address+count<=119) {
        uint32_t counters[4]={0}; uint16_t words[9]={0};
        if (s->log_stats) {
            s->log_stats(s->log_stats_ctx,counters); words[0]=1;
            for (unsigned i=0;i<4;i++) {
                words[1+2*i]=(uint16_t)(counters[i]>>16);
                words[2+2*i]=(uint16_t)counters[i];
            }
        }
        for (uint16_t i=0;i<count;i++) values[i]=words[address-110+i];
        return TP_OK;
    }
    if (s && s->controller && values && count && address>=53 && (uint32_t)address+count<=65) {
        uint16_t words[12]; log_words(s,words);
        for(uint16_t i=0;i<count;i++) values[i]=words[address-53+i];
        return TP_OK;
    }    if (!s || !s->controller || !values || !count ||
        !((address <= 5 && (uint32_t)address + count <= 6) ||
          (address >= 10 && (uint32_t)address + count <= 20))) return TP_INVALID;
    for (uint16_t i = 0; i < count; i++) values[i] = read_one(s, (uint16_t)(address + i));
    return TP_OK;
}
tp_result tp_service_write_holding(void *ctx, uint16_t address, uint16_t count, const uint16_t *v) {
    tp_service *s = ctx;
    if (!s || !s->controller || !v) return TP_INVALID;
    if (address == 0 && count == 1) {
        switch (v[0]) {
        case 1: return start(s);
        case 2:
            if (calibration_running(s)) tp_auto_cal_cancel(&s->auto_cal);
            else if (standard_running(s)) tp_auto_standard_cancel(&s->auto_standard);
            else tp_stop(s->controller);
            return TP_OK;
        case 3:
            if (s->controller->state!=TP_FAILED) return TP_INVALID;
            /* Keep the failure latched until the configured checks pass. */
            if (s->preflight) {
                s->controller->hal.safe_outputs(s->controller->hal.ctx);
                if (run_preflight(s)!=TP_OK) return TP_IO_ERROR;
            }
            return tp_clear_fault(s->controller);
        case 5: return s->maintenance ? TP_BUSY : tp_advance(s->controller, s->now_ms);
        case 6: return start_auto_cal(s);
        case 7: return apply_auto_cal(s);
        case 9: return start_auto_standard(s);
        case 10:
            if (!s->preflight) return TP_INVALID;
            if (s->maintenance || s->controller->state==TP_RUNNING ||
                calibration_running(s) || standard_running(s)) return TP_BUSY;
            return run_preflight(s);
        case 8:
            if (!s->recover_adc) return TP_INVALID;
            if (s->maintenance || s->controller->state==TP_RUNNING || calibration_running(s) ||
                standard_running(s)) return TP_BUSY;
            s->controller->hal.safe_outputs(s->controller->hal.ctx);
            return s->recover_adc(s->recover_ctx);
        case 4:
            if (!s->request_save || s->save_status==2 || s->controller->state==TP_RUNNING ||
                calibration_running(s) || standard_running(s)) return TP_BUSY;
            if (s->request_save(s->save_ctx,&s->config)!=TP_OK) { s->save_status=4; return TP_IO_ERROR; }
            s->save_status=2; return TP_OK;
        default: return TP_INVALID;
        }
    }
    /* Atomic FC16 timestamp, seconds since Unix epoch, high word first. */
    if (address==123 && count==2) {
        if (!s->set_clock) return TP_INVALID;
        if (s->maintenance || s->controller->state==TP_RUNNING || calibration_running(s) || standard_running(s)) return TP_BUSY;
        tp_result r=s->set_clock(s->clock_ctx,join(v[0],v[1]));
        if (r==TP_OK) { s->clock_valid=false; s->clock_utc=0; }
        return r;
    }
    if(address==50 && count==3) {
        if(!s->request_log || v[0]>1 || (v[0]==0 && (v[1] || v[2]))) return TP_INVALID;
        if(s->log_status==2) return TP_BUSY;
        tp_result r=s->request_log(s->log_ctx,v[0]==0,join(v[1],v[2]));
        if(r!=TP_OK) return r;
        memset(&s->log_event,0,sizeof(s->log_event)); s->log_status=2; return TP_OK;
    }
    if (address==20 && count==27) return set_config_words(s,v);
    if (address==70 && count==18) return calibrate_words(s,v);
    if (address==92 && count==10) return check_standard_words(s,v);
    /* The schedule block must be written in one FC16 request; no partial changes. */
    if (address != 1 || count != 5 || v[0] > TP_MODE_DAILY || s->save_status == 2) return TP_INVALID;
    if (calibration_running(s) || standard_running(s)) return TP_BUSY;
    uint32_t seconds = join(v[1], v[2]);
    if (seconds > 0x7fffffffu / 1000u || (s->request_save && !seconds)) return TP_INVALID;
    int32_t signed_offset = v[4] <= 32767u ? v[4] : (int32_t)v[4] - 65536;
    tp_result result = tp_schedule_configure(&s->schedule, (tp_mode)v[0], seconds * 1000u,
        v[3], (int16_t)signed_offset, s->now_ms, s->controller->state == TP_RUNNING);
    if (result == TP_OK && s->request_save) {
        s->config.period_seconds = seconds;
        s->config.mode = (uint8_t)v[0];
        s->config.daily_minute = v[3];
        s->config.utc_offset_minutes = (int16_t)signed_offset;
        s->save_status = 1;
    }
    return result;
}



