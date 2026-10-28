#ifndef TP_F407_STATUS_INPUT_H
#define TP_F407_STATUS_INPUT_H
#include <stdbool.h>
/* PC12, pulled low when the line is disconnected. */
void tp_f407_status_input_init(void);
bool tp_f407_status_input_ok(void);
#endif
