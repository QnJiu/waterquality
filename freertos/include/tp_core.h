#ifndef TP_CORE_H
#define TP_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Millisecond clocks are unsigned and may wrap. All durations must be < 2^31. */
typedef enum { TP_OK, TP_BUSY, TP_INVALID, TP_IO_ERROR, TP_TIMEOUT, TP_LOCK_BUSY, TP_UNSUPPORTED } tp_result;
typedef enum { TP_FAULT_NONE, TP_FAULT_STOP, TP_FAULT_INTERLOCK, TP_FAULT_SENSOR,
    TP_FAULT_OVERTEMP, TP_FAULT_PUMP, TP_FAULT_TIMEOUT, TP_FAULT_CALIBRATION,
    TP_FAULT_IO, TP_FAULT_RECIPE } tp_fault;
typedef enum { TP_IDLE, TP_RUNNING, TP_COMPLETE, TP_FAILED } tp_run_state;
typedef enum { TP_STEP_VALVES, TP_STEP_PUMP, TP_STEP_HEAT, TP_STEP_HOLD,
    TP_STEP_WAIT, TP_STEP_READ, TP_STEP_CALCULATE, TP_STEP_HEATER_OFF, TP_STEP_END } tp_step_kind;
typedef enum { TP_READ_BLANK, TP_READ_REACTED, TP_READ_TURBIDITY, TP_READ_COUNT } tp_reading;

typedef struct {
    tp_step_kind kind;
    uint32_t timeout_ms;
    union {
        struct { uint32_t mask; } valves;
        struct { int32_t steps; uint32_t frequency_hz; } pump;
        struct { float target_c; float tolerance_c; } heat;
        struct { uint32_t duration_ms; } wait;
        struct { tp_reading slot; uint8_t channel; uint8_t samples; } read;
    } arg;
} tp_step;

typedef struct {
    float kp, ki, kd, deadband_c, output_max, integral_max;
    float integral, previous_temp;
    bool initialized;
} tp_pid;

typedef struct {
    float slope, intercept, turbidity_factor;
    float min_signal, max_concentration;
} tp_calibration;

typedef struct {
    void *ctx;
    /* All callbacks are nonblocking. pump_poll reports completed pulses, not elapsed time. */
    tp_result (*set_valves)(void *, uint32_t mask);
    tp_result (*pump_start)(void *, int32_t steps, uint32_t frequency_hz);
    tp_result (*pump_poll)(void *, bool *done);
    tp_result (*read_temperature)(void *, float *degrees_c);
    tp_result (*set_heater)(void *, float fraction);
    tp_result (*read_adc)(void *, uint8_t channel, float *value);
    bool (*interlocks_ok)(void *);
    /* Must stop pump and heater and put all valves into the configured safe state. */
    void (*safe_outputs)(void *);
} tp_hal;

typedef struct {
    tp_hal hal;
    const tp_step *recipe;
    size_t recipe_len, index;
    tp_run_state state;
    tp_fault fault;
    uint32_t entered_ms, last_pid_ms, last_sample_ms;
    bool entered, heating, result_valid, single_step, awaiting_advance;
    uint32_t advance_wait_ms;
    float target_c, max_temperature_c, result_mg_l;
    float readings[TP_READ_COUNT], sample_sum;
    uint8_t valid_readings, sample_count;
    tp_pid pid;
    tp_calibration calibration;
} tp_controller;

void tp_init(tp_controller *c, const tp_hal *hal, const tp_calibration *cal,
             const tp_pid *pid, float max_temperature_c);
tp_result tp_start(tp_controller *c, const tp_step *recipe, size_t count, uint32_t now);
/* Single-step can be enabled immediately after a successful start, before tick.
 * At each boundary a 30-second wait is enforced while interlocks and temperature
 * regulation remain active. Stop and faults always force safe outputs. */
tp_result tp_advance(tp_controller *c, uint32_t now_ms);
void tp_tick(tp_controller *c, uint32_t now);
void tp_stop(tp_controller *c);
tp_result tp_clear_fault(tp_controller *c);
float tp_pid_update(tp_pid *pid, float target, float measured, float dt_seconds);
tp_result tp_concentration(const tp_calibration *cal, float blank, float reacted,
                           float turbidity, float *result);
#endif


