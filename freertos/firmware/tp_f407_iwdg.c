#include "tp_f407_iwdg.h"
#include "stm32f4xx.h"
tp_result tp_f407_iwdg_start(void) {
    if(SystemCoreClock!=16000000u) return TP_INVALID;
    IWDG->KR=0xccccu; /* also starts LSI */
    IWDG->KR=0x5555u;
    IWDG->PR=6u; /* divide by 256 */
    IWDG->RLR=1250u;
    uint32_t limit=1000000u;
    while((IWDG->SR & (IWDG_SR_PVU|IWDG_SR_RVU)) && --limit) { }
    if(!limit) return TP_TIMEOUT;
    IWDG->KR=0xaaaau;
    return TP_OK;
}
void tp_f407_iwdg_feed(void) { IWDG->KR=0xaaaau; }
