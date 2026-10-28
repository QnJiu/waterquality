#ifndef TP_F030_BOARD_INPUTS_H
#define TP_F030_BOARD_INPUTS_H
#include "tp_subordinate_node.h"
/* Return a coherent input snapshot. Unknown or failed inputs return an error. */
tp_result tp_f030_board_inputs(void *ctx, tp_sub_inputs *inputs);
#endif
