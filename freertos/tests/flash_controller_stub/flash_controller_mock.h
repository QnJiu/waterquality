#ifndef FLASH_CONTROLLER_MOCK_H
#define FLASH_CONTROLLER_MOCK_H
uint32_t hw_status(void);
uint32_t hw_control(void);
void hw_set_control(uint32_t);
void hw_clear_status(uint32_t);
void hw_unlock(void);
uint8_t hw_read(uint32_t);
void hw_program(uint32_t,uint8_t);
void hw_cache_reset(void);
#endif
