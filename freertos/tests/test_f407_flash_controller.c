#include "tp_f407_flash_controller.h"
#include "stm32f4xx.h"
#include <assert.h>
#include <string.h>
static uint8_t memory[2*TP_F407_IMAGE_BANK_SIZE];
static uint32_t control,status;
static unsigned erase_count,program_count,cache_count,last_sector;
static bool unlock_fail, stuck, inject_error, corrupt;
uint32_t hw_status(void) { return status; }
uint32_t hw_control(void) { return control; }
void hw_set_control(uint32_t v) {
    control=v;
    if(v&FLASH_CR_STRT) {
        last_sector=(v&FLASH_CR_SNB)>>FLASH_CR_SNB_Pos; erase_count++;
        if(stuck) status|=FLASH_SR_BSY;
        else if(inject_error) status|=FLASH_SR_WRPERR;
        else { memset(memory+(last_sector-5)*128u*1024u,0xff,128u*1024u); status|=FLASH_SR_EOP; }
        control&=~FLASH_CR_STRT;
    }
}
void hw_clear_status(uint32_t bits) { status&=~bits; }
void hw_unlock(void) { if(!unlock_fail) control&=~FLASH_CR_LOCK; }
uint8_t hw_read(uint32_t address) { return memory[address-TP_F407_IMAGE0_BASE]; }
void hw_program(uint32_t address,uint8_t value) {
    program_count++;
    if(stuck) status|=FLASH_SR_BSY;
    else if(inject_error) status|=FLASH_SR_PGSERR;
    else { memory[address-TP_F407_IMAGE0_BASE]&=corrupt ? (uint8_t)(value&0xfe) : value; status|=FLASH_SR_EOP; }
}
void hw_cache_reset(void) { cache_count++; }
static void reset(void) {
    memset(memory,0,sizeof(memory)); control=FLASH_CR_LOCK; status=0;
    erase_count=program_count=cache_count=last_sector=0;
    unlock_fail=stuck=inject_error=corrupt=false;
}
int main(void) {
    reset(); tp_f407_flash_controller ctl; tp_f407_flash_ops ops; tp_f407_image_flash image;
    assert(tp_f407_flash_controller_init(&ctl,3,&ops)==TP_OK);
    assert(tp_f407_image_flash_init(&image,&ops,0)==TP_OK);
    assert(ops.erase_sector(ops.ctx,4)==TP_INVALID);
    assert(tp_f407_image_erase(&image,1)==TP_OK && erase_count==3 && last_sector==10);
    assert((control&FLASH_CR_LOCK) && !(control&(FLASH_CR_PG|FLASH_CR_SER)) && cache_count==3);
    uint8_t input[]={0x71,0x42},output[2];
    assert(tp_f407_image_write(&image,1,0,input,2)==TP_OK && program_count==2);
    assert(tp_f407_image_read(&image,1,0,output,2)==TP_OK && !memcmp(input,output,2));
    assert(tp_f407_image_write(&image,1,0,input,2)==TP_OK && program_count==2);
    reset(); unlock_fail=true;
    assert(ops.erase_sector(ops.ctx,8)==TP_IO_ERROR && !erase_count);
    reset(); inject_error=true;
    assert(ops.erase_sector(ops.ctx,8)==TP_IO_ERROR && (control&FLASH_CR_LOCK) && cache_count==1);
    reset(); memset(memory,0xff,sizeof(memory)); inject_error=true;
    assert(ops.program_byte(ops.ctx,TP_F407_IMAGE1_BASE,1)==TP_IO_ERROR && (control&FLASH_CR_LOCK));
    reset(); memset(memory,0xff,sizeof(memory)); corrupt=true;
    assert(ops.program_byte(ops.ctx,TP_F407_IMAGE1_BASE,1)==TP_IO_ERROR);
    reset(); stuck=true;
    assert(ops.erase_sector(ops.ctx,8)==TP_TIMEOUT && ctl.poisoned);
    assert(ops.erase_sector(ops.ctx,9)==TP_INVALID && erase_count==1);
    assert(ops.read(ops.ctx,TP_F407_IMAGE1_BASE,output,1)==TP_INVALID);
    return 0;
}
