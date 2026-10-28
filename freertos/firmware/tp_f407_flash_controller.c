#include "tp_f407_flash_controller.h"
#include "stm32f4xx.h"
#include <stdint.h>

#define FLASH_ERRORS (FLASH_SR_SOP|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR)
#define FLASH_CLEAR (FLASH_ERRORS|FLASH_SR_EOP)
#define FLASH_MODES (FLASH_CR_PG|FLASH_CR_SER|FLASH_CR_SNB|FLASH_CR_PSIZE)
#ifdef TP_F407_FLASH_TEST
#include "flash_controller_mock.h"
#else
static uint32_t hw_status(void) { return FLASH->SR; }
static uint32_t hw_control(void) { return FLASH->CR; }
static void hw_set_control(uint32_t value) { FLASH->CR=value; }
static void hw_clear_status(uint32_t flags) { FLASH->SR=flags; }
static void hw_unlock(void) { FLASH->KEYR=0x45670123u; FLASH->KEYR=0xcdef89abu; }
static uint8_t hw_read(uint32_t address) { return *(volatile const uint8_t *)(uintptr_t)address; }
static void hw_program(uint32_t address,uint8_t value) { *(volatile uint8_t *)(uintptr_t)address=value; }
static void hw_cache_reset(void) {
    uint32_t acr=FLASH->ACR;
    FLASH->ACR=acr & ~(FLASH_ACR_ICEN|FLASH_ACR_DCEN);
    FLASH->ACR=(acr & ~(FLASH_ACR_ICEN|FLASH_ACR_DCEN)) | FLASH_ACR_ICRST|FLASH_ACR_DCRST;
    FLASH->ACR=acr & ~(FLASH_ACR_ICEN|FLASH_ACR_DCEN|FLASH_ACR_ICRST|FLASH_ACR_DCRST);
    FLASH->ACR=acr & ~(FLASH_ACR_ICRST|FLASH_ACR_DCRST);
    __DSB(); __ISB();
}
#endif
static bool range(uint32_t address,size_t n) {
    uint32_t start=TP_F407_IMAGE0_BASE, end=TP_F407_IMAGE1_BASE+TP_F407_IMAGE_BANK_SIZE;
    return address>=start && address<=end && n<=end-address;
}
static tp_result wait_ready(tp_f407_flash_controller *f) {
    for(uint32_t i=0;i<f->poll_limit;i++) if(!(hw_status()&FLASH_SR_BSY)) return TP_OK;
    /* Timeout leaves operation status uncertain; require a system reset before reuse. */
    f->poisoned=true; return TP_TIMEOUT;
}
static tp_result begin(tp_f407_flash_controller *f) {
    if(!f || f->poisoned || !f->poll_limit) return TP_INVALID;
    tp_result r=wait_ready(f); if(r!=TP_OK) return r;
    if(hw_control()&FLASH_CR_LOCK) hw_unlock();
    if(hw_control()&FLASH_CR_LOCK) return TP_IO_ERROR;
    hw_set_control(hw_control() & ~FLASH_MODES);
    hw_clear_status(FLASH_CLEAR);
    return TP_OK;
}
static tp_result finish(tp_f407_flash_controller *f) {
    tp_result r=wait_ready(f); if(r!=TP_OK) return r;
    uint32_t status=hw_status();
    hw_set_control((hw_control() & ~FLASH_MODES) | FLASH_CR_LOCK);
    hw_cache_reset(); hw_clear_status(FLASH_CLEAR);
    return (status&FLASH_ERRORS) ? TP_IO_ERROR : TP_OK;
}
static tp_result read_flash(void *v,uint32_t address,void *out,size_t n) {
    tp_f407_flash_controller *f=v;
    if(!f || f->poisoned || !range(address,n) || (!out && n)) return TP_INVALID;
    tp_result r=wait_ready(f); if(r!=TP_OK) return r;
    uint8_t *p=out; for(size_t i=0;i<n;i++) p[i]=hw_read(address+(uint32_t)i);
    return TP_OK;
}
static tp_result erase_flash(void *v,unsigned sector) {
    tp_f407_flash_controller *f=v;
    if(sector<5 || sector>10) return TP_INVALID;
    tp_result r=begin(f); if(r!=TP_OK) return r;
    uint32_t cr=(hw_control() & ~FLASH_MODES) | FLASH_CR_SER | ((uint32_t)sector<<FLASH_CR_SNB_Pos);
    hw_set_control(cr); hw_set_control(cr|FLASH_CR_STRT);
    return finish(f);
}
static tp_result program_flash(void *v,uint32_t address,uint8_t value) {
    tp_f407_flash_controller *f=v;
    if(!range(address,1)) return TP_INVALID;
    tp_result r=begin(f); if(r!=TP_OK) return r;
    uint8_t current=hw_read(address);
    if((current&value)!=value) { hw_set_control(hw_control()|FLASH_CR_LOCK); return TP_INVALID; }
    if(current==value) { hw_set_control(hw_control()|FLASH_CR_LOCK); return TP_OK; }
    hw_set_control((hw_control() & ~FLASH_MODES) | FLASH_CR_PG); /* PSIZE=byte */
    hw_program(address,value);
    r=finish(f); if(r!=TP_OK) return r;
    return hw_read(address)==value ? TP_OK : TP_IO_ERROR;
}
tp_result tp_f407_flash_controller_init(tp_f407_flash_controller *f,uint32_t poll_limit,tp_f407_flash_ops *ops) {
    if(!f || !ops || !poll_limit) return TP_INVALID;
    f->poll_limit=poll_limit; f->poisoned=false;
    *ops=(tp_f407_flash_ops){f,read_flash,erase_flash,program_flash}; return TP_OK;
}
