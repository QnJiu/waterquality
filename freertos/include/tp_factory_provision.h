#ifndef TP_FACTORY_PROVISION_H
#define TP_FACTORY_PROVISION_H
#include "tp_signed_boot.h"
/* Trusted factory only: image must already be written, and all operations must
 * be serialized. No erase/program of image or boot metadata recovery occurs.
 * Identical completed provisioning is verified and may be retried. */
tp_result tp_factory_provision(tp_signed_boot *signed_boot,const tp_store_io *metadata,
                               const uint8_t *manifest,size_t manifest_size);
#endif
