#ifndef TP_FREEMODBUS_TEST_PORT_H
#define TP_FREEMODBUS_TEST_PORT_H
#include <stdint.h>
#include <assert.h>
typedef uint8_t BOOL;
typedef uint8_t UCHAR;
typedef char CHAR;
typedef uint16_t USHORT;
typedef int16_t SHORT;
typedef uint32_t ULONG;
typedef int32_t LONG;
#define TRUE 1
#define FALSE 0
#define ENTER_CRITICAL_SECTION()
#define EXIT_CRITICAL_SECTION()
#define MB_ASCII_ENABLED 0
#define MB_TCP_ENABLED 0
#define MB_FUNC_OTHER_REP_SLAVEID_ENABLED 0
#define MB_FUNC_READ_INPUT_ENABLED 0
#define MB_FUNC_READ_COILS_ENABLED 0
#define MB_FUNC_WRITE_COIL_ENABLED 0
#define MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED 0
#define MB_FUNC_READ_DISCRETE_INPUTS_ENABLED 0
#define MB_FUNC_READWRITE_HOLDING_ENABLED 0
#endif
