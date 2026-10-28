#include "tp_f030_pn532_door.h"

tp_result tp_f030_pn532_door_init(tp_f030_pn532_door *d,const tp_pn532_i2c_bus *bus,
                                  const tp_f030_door_config *config) {
    if(!d || !bus || !config) return TP_INVALID;
    tp_result r=tp_pn532_i2c_init(&d->transport,bus);
    if(r!=TP_OK) return r;
    tp_pn532_io reader=tp_pn532_i2c_io(&d->transport);
    r=tp_door_service_init(&d->service,&reader,&config->lock,config->request_ctx,
                           config->request,config->command_timeout_ms,config->scan_interval_ms,
                           config->scan_retries,config->unlock_ms,config->freshness_ms,
                           config->first_request_id);
    if(r!=TP_OK) tp_pn532_i2c_abort(&d->transport);
    return r;
}
tp_result tp_f030_pn532_door_recover(tp_f030_pn532_door *d) {
    if(!d || d->service.door.state!=TP_DOOR_FAULT ||
       d->transport.state!=TP_PN532_I2C_FAULT) return TP_INVALID;
    tp_pn532_i2c_bus bus=d->transport.bus;
    tp_result r=tp_pn532_i2c_init(&d->transport,&bus);
    if(r!=TP_OK) return r;
    r=tp_door_service_recover(&d->service);
    if(r!=TP_OK) tp_pn532_i2c_abort(&d->transport);
    return r;
}
void tp_f030_pn532_door_abort(void *ctx) {
    tp_f030_pn532_door *d=ctx;
    if(d && d->transport.state!=TP_PN532_I2C_FAULT) tp_pn532_i2c_abort(&d->transport);
}
