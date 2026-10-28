#include "tp_f407_board_config.h"
#include "tp_preflight.h"

/* Teaching example only: none of these values describe a verified instrument. */
enum { SAMPLE = 1u, REAGENT = 2u, RINSE = 4u, DRAIN = 8u,
       STANDARD_LOW = 16u, STANDARD_HIGH = 32u };
static const tp_step recipe[] = {
    {.kind=TP_STEP_VALVES, .arg.valves={SAMPLE}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_BLANK,0,3}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_TURBIDITY,2,3}},
    {.kind=TP_STEP_VALVES, .arg.valves={REAGENT}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_HEAT, .timeout_ms=20000, .arg.heat={30,0.5f}},
    {.kind=TP_STEP_HOLD, .timeout_ms=1000, .arg.wait={1000}},
    {.kind=TP_STEP_HEATER_OFF},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_REACTED,1,3}},
    {.kind=TP_STEP_CALCULATE},
    {.kind=TP_STEP_VALVES, .arg.valves={RINSE}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_VALVES, .arg.valves={DRAIN}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_END}
};
/* Demonstration only: real standard handling and reaction steps need a method. */
static const tp_step low_standard[] = {
    {.kind=TP_STEP_VALVES, .arg.valves={STANDARD_LOW}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_BLANK,0,3}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_TURBIDITY,2,3}},
    {.kind=TP_STEP_VALVES, .arg.valves={REAGENT}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_HEAT, .timeout_ms=20000, .arg.heat={30,0.5f}},
    {.kind=TP_STEP_HOLD, .timeout_ms=1000, .arg.wait={1000}},
    {.kind=TP_STEP_HEATER_OFF},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_REACTED,1,3}},
    {.kind=TP_STEP_VALVES, .arg.valves={RINSE}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_VALVES, .arg.valves={DRAIN}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_END}
};
static const tp_step high_standard[] = {
    {.kind=TP_STEP_VALVES, .arg.valves={STANDARD_HIGH}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_BLANK,0,3}},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_TURBIDITY,2,3}},
    {.kind=TP_STEP_VALVES, .arg.valves={REAGENT}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_HEAT, .timeout_ms=20000, .arg.heat={30,0.5f}},
    {.kind=TP_STEP_HOLD, .timeout_ms=1000, .arg.wait={1000}},
    {.kind=TP_STEP_HEATER_OFF},
    {.kind=TP_STEP_READ, .timeout_ms=1000, .arg.read={TP_READ_REACTED,1,3}},
    {.kind=TP_STEP_VALVES, .arg.valves={RINSE}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_VALVES, .arg.valves={DRAIN}},
    {.kind=TP_STEP_PUMP, .timeout_ms=3000, .arg.pump={100,100}},
    {.kind=TP_STEP_END}
};
static const tp_f407_auto_cal_options auto_calibration = {
    low_standard, sizeof(low_standard)/sizeof(low_standard[0]),
    high_standard, sizeof(high_standard)/sizeof(high_standard[0]), 0, 5, 0.01f
};
static const tp_f407_auto_standard_options auto_standard = {
    high_standard, sizeof(high_standard)/sizeof(high_standard[0]), 5, 0.05f
};
/* Illustrate mutually exclusive flow paths; a real policy requires a schematic. */
static const tp_valve_policy valve_policy = {
    .forbidden = {REAGENT|RINSE|DRAIN|STANDARD_LOW|STANDARD_HIGH,
                  SAMPLE|RINSE|DRAIN|STANDARD_LOW|STANDARD_HIGH,
                  SAMPLE|REAGENT|DRAIN|STANDARD_LOW|STANDARD_HIGH,
                  SAMPLE|REAGENT|RINSE|STANDARD_LOW|STANDARD_HIGH,
                  SAMPLE|REAGENT|RINSE|DRAIN|STANDARD_HIGH,
                  SAMPLE|REAGENT|RINSE|DRAIN|STANDARD_LOW}
};
/* Placeholder MUX and scale values, not a verified ADS1220 configuration. */
static const tp_adc_channel channels[] = {
    {0, 1.0f / 8388608.0f}, {1, 1.0f / 8388608.0f},
    {2, 1.0f / 8388608.0f}
};
static bool interlocks_ok(void *ctx) { (void)ctx; return false; }
static uint16_t resource_faults(void *ctx) {
    (void)ctx; return TP_PREFLIGHT_RESOURCE_UNKNOWN;
}

tp_result tp_f407_board_options(tp_f407_app_options *options) {
    if (!options) return TP_INVALID;
    *options = (tp_f407_app_options){
        .recipe=recipe, .recipe_len=sizeof(recipe)/sizeof(recipe[0]),
        .valves=&valve_policy, .interlocks_ok=interlocks_ok,
        .resource_faults=resource_faults,
        .channels=channels, .channel_count=sizeof(channels)/sizeof(channels[0]),
        .auto_calibration=&auto_calibration, .auto_standard=&auto_standard,
        .adc_registers={0,0,0,0}, .adc_timeout_ms=1000,
        .defaults={.calibration={1,0,0,0.01f,10}, .kp=0.2f, .ki=0.02f,
                   .output_max=1, .integral_max=20, .max_temperature_c=80,
                   .period_seconds=3600, .slave_address=1}
    };
    /* Replace this example with a board-specific implementation after verifying
     * wiring, method and safety. Returning an error keeps the firmware halted. */
    return TP_INVALID;
}
