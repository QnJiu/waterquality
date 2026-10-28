#ifndef TP_AUTO_CALIBRATION_H
#define TP_AUTO_CALIBRATION_H
#include "tp_core.h"

/* The same owner must exclusively operate the controller for the whole run.
 * Recipes and their contents must remain valid and unchanged until completion.
 * Each recipe prepares one standard, reads each optical slot, and ends safely.
 * It must not contain a concentration calculation. */
typedef enum {
    TP_AUTO_CAL_IDLE, TP_AUTO_CAL_LOW, TP_AUTO_CAL_HIGH,
    TP_AUTO_CAL_COMPLETE, TP_AUTO_CAL_FAILED, TP_AUTO_CAL_CANCELLED
} tp_auto_cal_state;
typedef struct {
    tp_controller *controller;
    const tp_step *recipes[2];
    size_t lengths[2];
    tp_calibration base, fitted;
    tp_calibration_point points[2];
    float min_delta;
    tp_auto_cal_state state;
    tp_result error;
    void *check_ctx;
    /* Optional nonblocking safety/preflight check, called before each run. */
    tp_result (*check)(void *ctx);
} tp_auto_calibration;

/* Initialization does not start the controller. Inputs are copied except recipes. */
tp_result tp_auto_cal_init(tp_auto_calibration *a, tp_controller *c,
    const tp_step *low, size_t low_len, const tp_step *high, size_t high_len,
    float low_concentration, float high_concentration, float min_delta,
    void *check_ctx, tp_result (*check)(void *));
tp_result tp_auto_cal_start(tp_auto_calibration *a, uint32_t now);
/* Drives tp_tick; do not also tick the controller elsewhere. */
void tp_auto_cal_tick(tp_auto_calibration *a, uint32_t now);
void tp_auto_cal_cancel(tp_auto_calibration *a);
/* Result is available only after both runs and fitting succeed. Applying and
 * persisting it are explicit owner actions. Output remains unchanged on error. */
tp_result tp_auto_cal_result(const tp_auto_calibration *a, tp_calibration *out);
/* One standard recipe prepares the liquid, reads all three optical slots and
 * ends safely. The owner must exclusively drive the controller for the run. */
typedef enum {
    TP_STANDARD_IDLE, TP_STANDARD_RUNNING, TP_STANDARD_COMPLETE,
    TP_STANDARD_FAILED, TP_STANDARD_CANCELLED
} tp_standard_state;
typedef struct {
    tp_controller *controller;
    const tp_step *recipe;
    size_t length;
    tp_calibration calibration;
    float concentration, tolerance, actual;
    bool passed;
    tp_standard_state state;
    tp_result error;
    void *check_ctx;
    tp_result (*check)(void *ctx);
} tp_auto_standard;
/* Recipes must remain valid and unchanged throughout the run. Calibration is
 * copied at initialization; reinitialize after changing it. */
tp_result tp_auto_standard_init(tp_auto_standard *, tp_controller *,
    const tp_step *, size_t, float concentration, float tolerance,
    void *check_ctx, tp_result (*check)(void *));
tp_result tp_auto_standard_start(tp_auto_standard *, uint32_t now);
/* Drives the controller; do not also tick it elsewhere. */
void tp_auto_standard_tick(tp_auto_standard *, uint32_t now);
void tp_auto_standard_cancel(tp_auto_standard *);
/* Outputs remain unchanged unless the check completed successfully. */
tp_result tp_auto_standard_result(const tp_auto_standard *, float *actual, bool *passed);
#endif
