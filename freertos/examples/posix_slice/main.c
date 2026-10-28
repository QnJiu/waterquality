#include "FreeRTOS.h"
#include "task.h"
#include "tp_adc_status.h"
#include "adc_slice_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fake SPI values reproduce the Zephyr native_sim teaching vector. */
typedef struct { uint8_t regs[4]; } fake_adc;
static tp_result transfer(void *ctx,const uint8_t *tx,uint8_t *rx,size_t n) {
    fake_adc *f=ctx; memset(rx,0,n);
    if(n==5 && tx[0]==0x43) memcpy(f->regs,tx+1,4);
    else if(n==5 && tx[0]==0x23) memcpy(rx+1,f->regs,4);
    else if(n==2 && tx[0]==0x40) f->regs[0]=tx[1];
    else if(n==4 && tx[0]==0x10) {
        rx[1]=(uint8_t)(TP_SLICE_RAW>>16); rx[2]=(uint8_t)(TP_SLICE_RAW>>8); rx[3]=(uint8_t)TP_SLICE_RAW;
    } else if(n!=1 || (tx[0]!=0x08 && tx[0]!=0x02)) return TP_IO_ERROR;
    return TP_OK;
}
static tp_result ready(void *ctx,bool *out) { (void)ctx; *out=true; return TP_OK; }
static uint16_t crc16(const uint8_t *data,size_t len) {
    uint16_t crc=0xffff;
    while(len--) { crc^=*data++; for(unsigned i=0;i<8;i++)
        crc=(uint16_t)((crc>>1)^((crc&1u)?0xa001u:0)); }
    return crc;
}
static void run_slice(void *arg) {
    (void)arg;
    fake_adc fake={0}; tp_ads1220 adc; tp_ads1220_io io={&fake,transfer,ready};
    uint8_t regs[4]={1,0,0,0}; tp_adc_channel channel={8,TP_SLICE_VOLTS_PER_COUNT};
    tp_adc_reader reader; tp_adc_status status;
    const uint8_t request[]=TP_SLICE_REQUEST;
    uint8_t response[32]; size_t length=0;
    if(tp_ads1220_init(&adc,&io,regs,100)!=TP_OK ||
       tp_adc_reader_init(&reader,&adc,&channel,1)!=TP_OK ||
       tp_adc_status_init(&status,&reader,0,1000,0)!=TP_OK) goto fail;
    tp_adc_status_poll(&status,(uint32_t)xTaskGetTickCount());
    vTaskDelay(pdMS_TO_TICKS(10));
    tp_adc_status_poll(&status,(uint32_t)xTaskGetTickCount());
    tp_modbus modbus={&status,1,tp_adc_status_read,NULL};
    if(tp_modbus_frame(&modbus,request,sizeof(request),response,sizeof(response),&length)!=TP_OK ||
       length!=17 || response[0]!=1 || response[1]!=3 || response[2]!=12 ||
       response[3]!=0 || response[4]!=1 || response[5]!=0 || response[6]!=0 ||
       response[7]!=0 || response[8]!=0 || response[9]!=0 || response[10]!=1 ||
       response[11]!=(uint8_t)(TP_SLICE_UV>>24) || response[12]!=(uint8_t)(TP_SLICE_UV>>16) ||
       response[13]!=(uint8_t)(TP_SLICE_UV>>8) || response[14]!=(uint8_t)TP_SLICE_UV ||
       crc16(response,15)!=(uint16_t)(response[15]|((uint16_t)response[16]<<8))) goto fail;
    const uint8_t snapshot_request[]=TP_SLICE_SNAPSHOT_REQUEST;
    if(tp_modbus_frame(&modbus,snapshot_request,sizeof(snapshot_request),response,sizeof(response),&length)!=TP_OK ||
       length!=21 || response[0]!=1 || response[1]!=3 || response[2]!=16 ||
       response[3]!=response[17] || response[4]!=response[18] ||
       response[5]!=0 || response[6]!=1 || response[7]!=0 || response[8]!=0 ||
       response[9]!=0 || response[10]!=0 || (response[11]==0 && response[12]==0) ||
       response[13]!=(uint8_t)(TP_SLICE_UV>>24) || response[14]!=(uint8_t)(TP_SLICE_UV>>16) ||
       response[15]!=(uint8_t)(TP_SLICE_UV>>8) || response[16]!=(uint8_t)TP_SLICE_UV ||
       crc16(response,19)!=(uint16_t)(response[19]|((uint16_t)response[20]<<8))) goto fail;
    puts("TP_FREERTOS_POSIX_SELFTEST PASS"); fflush(stdout); exit(0);
fail:
    fputs("TP_FREERTOS_POSIX_SELFTEST FAIL\n",stderr); exit(1);
}
int main(void) {
    if(xTaskCreate(run_slice,"slice",4096,NULL,1,NULL)!=pdPASS) return 1;
    vTaskStartScheduler();
    return 1;
}
