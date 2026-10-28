#ifndef TP_F407_BOARD_CONFIG_H
#define TP_F407_BOARD_CONFIG_H
#include "tp_f407_app.h"

/* Called once before the scheduler starts. Return TP_OK only after supplying
 * all options; referenced objects and callbacks must remain valid forever. */
tp_result tp_f407_board_options(tp_f407_app_options *options);
#endif
