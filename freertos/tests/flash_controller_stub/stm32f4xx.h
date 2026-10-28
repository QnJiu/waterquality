#ifndef STM32F4XX_H
#define STM32F4XX_H
#define FLASH_SR_EOP (1u<<0)
#define FLASH_SR_SOP (1u<<1)
#define FLASH_SR_WRPERR (1u<<4)
#define FLASH_SR_PGAERR (1u<<5)
#define FLASH_SR_PGPERR (1u<<6)
#define FLASH_SR_PGSERR (1u<<7)
#define FLASH_SR_BSY (1u<<16)
#define FLASH_CR_PG (1u<<0)
#define FLASH_CR_SER (1u<<1)
#define FLASH_CR_SNB (31u<<3)
#define FLASH_CR_SNB_Pos 3u
#define FLASH_CR_PSIZE (3u<<8)
#define FLASH_CR_STRT (1u<<16)
#define FLASH_CR_LOCK (1u<<31)
#endif
