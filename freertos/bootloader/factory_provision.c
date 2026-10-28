#include "tp_factory_provision.h"
#include <string.h>
tp_result tp_factory_provision(tp_signed_boot *signed_boot,const tp_store_io *metadata,
                               const uint8_t *manifest,size_t manifest_size) {
    if (!signed_boot || !metadata || !metadata->read || !metadata->write || !metadata->erase ||
        !signed_boot->manifests.read || !signed_boot->manifests.write || !signed_boot->manifests.erase)
        return TP_INVALID;
    tp_download_manifest m;
    tp_result r=tp_signed_boot_authenticate(signed_boot,manifest,manifest_size,&m);
    if (r!=TP_OK) return r;
    tp_boot_io boot={*metadata,signed_boot,tp_signed_boot_verify};
    tp_boot_state state;
    r=tp_boot_read(&boot,&state);
    if (r==TP_OK) {
        if (state.phase!=TP_BOOT_IDLE || state.active_bank!=m.bank || state.active_version!=m.version) return TP_BUSY;
        uint8_t stored[TP_SIGNED_MANIFEST_SIZE];
        r=signed_boot->manifests.read(signed_boot->manifests.ctx,m.bank,0,stored,sizeof(stored));
        if (r!=TP_OK) return r;
        if (memcmp(stored,manifest,sizeof(stored))) return TP_BUSY;
        return tp_signed_boot_verify(signed_boot,m.bank,m.version);
    }
    if (r!=TP_INVALID) return r;
    r=tp_boot_metadata_blank(&boot);
    if (r!=TP_OK) return r;
    /* Verifies the pre-written image before touching its manifest store. */
    r=tp_signed_boot_download_verify(signed_boot,&m);
    if (r!=TP_OK) return r;
    return tp_boot_provision(&boot,m.bank,m.version);
}
