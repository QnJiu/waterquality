#ifndef TP_SIGNED_STAGED_SOURCE_H
#define TP_SIGNED_STAGED_SOURCE_H
#include "tp_signed_manifest.h"
#include "tp_staged_copy.h"

/* Keep the source image, signed bytes and this context unchanged and exclusively
 * owned throughout copying. Configure signature with the trusted key, product
 * and source image reader before use. Recreate it after a reset. */
typedef struct {
    tp_signed_manifest signature;
    const uint8_t *manifest_bytes;
    size_t manifest_size;
} tp_signed_staged_source;

/* The returned callbacks authenticate the signed manifest and hash the entire
 * source before the destination can be erased. The destination's independent
 * signature context must also be authenticated with the same signed bytes. */
tp_staged_source tp_signed_staged_source_callbacks(tp_signed_staged_source *);
#endif
