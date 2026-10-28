#ifndef	MODBUS_H
#define MODBUS_H
/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
/* ----------------------- Defines ------------------------------------------*/
#define DISCRETE_INPUT_START        1
#define DISCRETE_INPUT_NDISCRETES   32
#define COIL_START                  1
#define COIL_NCOILS                 64
#define REG_INPUT_START             1
#define REG_INPUT_NREGS             512			// 原来是64
#define REG_HOLDING_START           1//4001
#define REG_HOLDING_NREGS           512			// 原来是256

#define REG_BLOCK_START           1//4001
#define REG_BLOCK_NREGS           8

#define FALSH_USER_START_ADDRESS    (uint32_t)0x0800FC00           //用于用户存数据使用的内部闪存的起始地址

//在保持寄存器中，各个地址对应的功能定义
#define          HD_SYSTEM_TASK_FLAG            0		  //系统任务标志
#define          HD_USER_PASSWORD_START         1         //用户密码起始地址（5个普通、1个超级管理员）
#define          HD_SUPER_ADMIN_PASSWORD        6         //超级管理员的密码
#define          HD_CUR_USER_PASSWORD		    7		  //当前用户输入的密码
#define          HD_CUR_USER_NUMBER			    8         //当前用户的编号
#define          HD_CUR_INTERFACE			    91         //当前界面
#define          HD_TIME_YEAR                  10		  //时间：年
#define          HD_TIME_MONTH                 11		  //时间：月
#define          HD_TIME_DAY                   12		  //时间：日
#define          HD_TIME_WEEK                  13		  //时间：星期
#define          HD_TIME_HOUR                  14		  //时间：时
#define          HD_TIME_MIN                   15		  //时间：分
#define          HD_TIME_SEC                   16		  //时间：秒

/*--------------------------Extern Functions------------------------------------*/
extern UCHAR xMBUtilGetBits( UCHAR * ucByteBuf, USHORT usBitOffset, UCHAR ucNBits );
extern void  xMBUtilSetBits( UCHAR * ucByteBuf, USHORT usBitOffset, UCHAR ucNBits,UCHAR ucValue );

void ModbusRegInit(uint8_t Fun);
 	
#endif
