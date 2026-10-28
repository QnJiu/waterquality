/*
 * hostcmd.h — 上位机命令处理 (完整命令表)
 */

#ifndef HOSTCMD_H
#define HOSTCMD_H

#include <stdint.h>

typedef enum {
	CMD_NONE = 0x00,

	CMD_SET_BAUD = 0x01,
	CMD_SET_LOCAL_ADD = 0x02,
	CMD_SET_SPECTRO_SCAN_TIMES = 0x03,
	CMD_SET_SPECTRO_INTEGRAL_TIME = 0x04,
	CMD_Reserve5 = 0x05,
	CMD_Reserve6 = 0x06,
	CMD_Reserve7 = 0x07,
	CMD_Reserve8 = 0x08,
	CMD_Reserve9 = 0x09,

	CMD_START_COMBI_VALVE    = 0x0A, /* 开组合阀 (位掩码) */
	CMD_STOP_COMBI_VALVE     = 0x0B, /* 关组合阀 */
	CMD_PERISTALTIC_PUMP_OPT = 0x0C, /* 操作蠕动泵 (速度+圈数) */
	CMD_DRAW_REAGENT         = 0x0D, /* 抽取试剂 */
	CMD_DETERMIN_LEVEL_DOWN  = 0x0E, /* 下行判定液位 */
	CMD_DETERMIN_LEVEL_UP    = 0x0F, /* 上行判定液位 */
	CMD_START_TEMPER_CTRL    = 0x10, /* 温控 (阻塞模式) */
	CMD_START_TEMPER_CTRL_CONST = 0x11, /* 温控 (非阻塞) */
	CMD_STOP_TEMPER_CTRL     = 0x12, /* 停止温控 */
	CMD_READ_DARK_LED        = 0x13, /* 读暗电流 */
	CMD_READ_LED             = 0x14, /* 读光强AD值 */

	CMD_SPECTRO_LIGHT_ON     = 0x15, /* 光谱仪开光源 */
	CMD_SPECTRO_LIGHT_OFF    = 0x16, /* 光谱仪关光源 */
	CMD_SPECTRO_SCAN         = 0x17, /* 光谱仪启动扫描 */
	CMD_SPECTRO_READ_DATA    = 0x18, /* 光谱仪读数据 */
	CMD_SYRINGE_RESET_ORIGIN = 0x19, /* 注射泵回零 */
	CMD_SYRINGE_MOVE         = 0x1A, /* 注射泵相对移动 */
	CMD_SYRINGE_MOVE_TO      = 0x1B, /* 注射泵绝对移动 */
	CMD_SYRINGE_STOP         = 0x1C, /* 注射泵停止 */
	CMD_STIR_ON              = 0x1D, /* 开启搅拌 */
	CMD_STIR_OFF             = 0x1E, /* 停止搅拌 */

	CMD_4_20_MA_CALIB_TEST   = 0x1F, /* 4-20mA 输出测试 */
	CMD_4_20_MA_CALIB_MAX    = 0x20, /* 4-20mA 校准上限 */
	CMD_4_20_MA_CALIB_MIN    = 0x21, /* 4-20mA 校准下限 */
	CMD_4_20_MA_NORMAL       = 0x22, /* 4-20mA 正常 */
	CMD_SYRINGE_READ_POS     = 0x23, /* 读注射泵当前位置 */

	CMD_RZ_HOLE              = 0x29, /* 润泽切换阀 */
	CMD_RZ_SPEED             = 0x2A, /* 润泽设置速度 */
	CMD_RZ_ML                = 0x2B, /* 润泽运行mL */
	CMD_RZ_OPEN              = 0x2C, /* 润泽开阀 */
	CMD_RZ_CLOSE             = 0x2D, /* 润泽关阀 */

	CMD_HC_HOLE              = 0x2E, /* HC切换阀 */
	CMD_HC_SPEED             = 0x2F, /* HC设置速度 */
	CMD_HC_ML                = 0x30, /* HC运行mL */
	CMD_HC_OPEN              = 0x31, /* HC开阀 */
	CMD_HC_CLOSE             = 0x32, /* HC关阀 */

	CMD_START_TEMP_CTRL      = 0x54, /* 温控 (配合恒温温度/恒温时长) */
	CMD_STOP_TEMP_CTRL       = 0x55, /* 停止温控 */
	CMD_START_ANALYSIS_PROC  = 0x57, /* 启动分析流程 */
	CMD_START_CALIBRATE      = 0x58, /* 自动标定 */
	CMD_START_CALIB_LOW      = 0x59, /* 低点校准 */
	CMD_START_CALIB_HIGH     = 0x5A, /* 高点校准 */
	CMD_START_CHECK_LOW      = 0x5B, /* 低点核查 */
	CMD_START_CHECK_HIGH     = 0x5C, /* 高点核查 */
	CMD_START_INIT_PROC      = 0x5D, /* 清洗流程 */

	CMD_PERISTALTIC_PUMP_DRAW  = 0x65, /* 蠕动泵抽 */
	CMD_PERISTALTIC_PUMP_PUSH  = 0x66, /* 蠕动泵推 */
	CMD_PERISTALTIC_PUMP_STOP  = 0x67, /* 停止蠕动泵 */
	CMD_VALVE_OPEN             = 0x68, /* 开启功能阀 */
	CMD_VALVE_CLOSE            = 0x69, /* 关闭功能阀 */
	CMD_WASH_POOL_OPT          = 0x6A, /* 清洗比色池 */
	CMD_ADD_REAGENT_TO_POOL_OPT = 0x6B, /* 向比色池加试剂 */
	CMD_LL_AD_CALIB            = 0x6C, /* 液位定量AD校准 */
	CMD_CLEAR_POOL             = 0x6D, /* 清空比色池 */
	CMD_CLEAR_SAMPLE_PIPE      = 0x6E, /* 清空样品管 */
	CMD_STIR_POOL              = 0x6F, /* 搅拌比色池 */
	CMD_REAGENTS_PROFILL       = 0x70, /* 试剂预充满 */
	CMD_INIT_SYRINGE_PUMP      = 0x71, /* 初始化注射泵 */
	CMD_UV_TEST                = 0x72, /* 紫外测试 */
	CMD_FIT_CURVE              = 0x73, /* 拟合曲线 */

	CMD_START_NH3N   = 0x78, /* 启动 NH3N 流程 */
	CMD_START_TP     = 0x79, /* 启动 TP 流程 */
	CMD_START_COD    = 0x7A, /* 启动 COD 流程 */
	CMD_START_TN     = 0x7B, /* 启动 TN 流程 */
	CMD_START_CODMN  = 0x7C, /* 启动 CODMN 流程 */
	CMD_RZSTART_NH3N  = 0x7D,
	CMD_RZSTART_TP    = 0x7E,
	CMD_RZSTART_COD   = 0x7F,
	CMD_RZSTART_TN    = 0x80,
	CMD_RZSTART_CODMN = 0x81,
	CMD_HCSTART_NH3N  = 0x82,
	CMD_HCSTART_TP    = 0x83,
	CMD_HCSTART_COD   = 0x84,
	CMD_HCSTART_TN    = 0x85,
	CMD_HCSTART_CODMN = 0x86,

	CMD_SET_PCB_DATE_TIME    = 0xC8, /* 设定时间 */
	CMD_GET_PCB_DATE_TIME    = 0xC9, /* 获取时间 */
	CMD_SAVE_DEV_OPERAT_PARA = 0xFC, /* 保存设备参数 */
	CMD_SLAVE_HALT           = 0xFD, /* 停机不复位 */
	CMD_SAVE_DATA            = 0xFE, /* 保存 Modbus 数据 */
	CMD_SLAVE_RESET          = 0xFF, /* 控制板重启 */

	CMD_MAX_CODE = 0x100,
} HOST_CMD_t;

void RS232CmdProcess(void);
void HostCmdProcess(void);

#endif /* HOSTCMD_H */
