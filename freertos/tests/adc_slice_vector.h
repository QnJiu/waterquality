#ifndef TP_ADC_SLICE_VECTOR_H
#define TP_ADC_SLICE_VECTOR_H
/* Shared teaching example: gain 1, internal 2.048 V reference. */
#define TP_SLICE_RAW 0x100000u
#define TP_SLICE_UV 256000u
#define TP_SLICE_VOLTS_PER_COUNT (2.048f / 8388608.0f)
#define TP_SLICE_REQUEST {1, 3, 0, 0, 0, 6, 0xc5, 0xc8}
#define TP_SLICE_SNAPSHOT_REQUEST {1, 3, 0, 6, 0, 8, 0xa4, 0x0d}
#endif
