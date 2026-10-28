/*
 * port.h — FreeModbus 兼容头文件
 *
 * 提供 FreeModbus 核心文件期望的基本类型定义 (BOOL, UCHAR 等)
 * 和 Zephyr 平台相关的函数/类型声明
 */

#ifndef FREEMODBUS_PORT_H
#define FREEMODBUS_PORT_H

/* 基本类型 (FreeModbus 核心所需) */
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  BOOL;    /**< 布尔类型 (FreeModbus 用 BOOL, 非 C99 bool) */
typedef uint8_t  UCHAR;   /**< 无符号字节 */
typedef int8_t   CHAR;    /**< 有符号字节 */
typedef uint16_t USHORT;  /**< 无符号 16 位 */
typedef int16_t  SHORT;   /**< 有符号 16 位 */
typedef uint32_t ULONG;   /**< 无符号 32 位 */
typedef int32_t  LONG;    /**< 有符号 32 位 */

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Zephyr 平台接口 (临界区/信号量/互斥锁/UART/定时器声明) */
#include "mb_port.h"
/* FreeModbus 核心类型 (eMBErrorCode, eMBRegisterMode 等) */
#include "mbport.h"

#endif /* FREEMODBUS_PORT_H */
