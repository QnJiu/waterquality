#ifndef TP_F030_STATUS_OUTPUT_H
#define TP_F030_STATUS_OUTPUT_H
#include "tp_subordinate_node.h"

/* PB0: high means the latest snapshot has no faults, low means fault. */
void tp_f030_status_output_init(void);
void tp_f030_status_output_update(tp_sub_node *node);
void tp_f030_status_output_fault(void);
#endif
