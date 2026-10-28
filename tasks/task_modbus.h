/*
 * task_modbus.h — Modbus 寄存器映射 + 全局数据声明
 */

#ifndef TASK_MODBUS_H
#define TASK_MODBUS_H

#include <stdint.h>
#include <stdbool.h>

/* Modbus 寄存器数量 */
#define REG_INPUT_START     1
#define REG_INPUT_NREGS     512
#define REG_HOLDING_START   1
#define REG_HOLDING_NREGS   512
#define COIL_NCOILS         64

/* ==================== 保持寄存器结构体 (读写, 4X) ==================== */

#pragma pack(push, 1)
typedef struct {
	/* PDU 地址 = buffer 索引 (FreeModbus 回调前 usRegAddress++, 实测确认) */
	uint16_t nHostCMD;              /* PDU 0 (0x00): 上位机命令 */
	uint16_t nBand;                 /* PDU 1 (0x01): 波特率/100 */
	uint16_t nLocalAddr;            /* PDU 2 (0x02): 从站地址 */
	uint16_t _pad_3_29[27];         /* PDU 3-29: 保留 */
	uint16_t nStirWorkTime;         /* PDU 30 (0x1E): 搅拌工作时间 (×10ms) */
	uint16_t nStirSleepTime;        /* PDU 31 (0x1F): 搅拌停止时间 (×10ms) */
	uint16_t nCoolFanValve;         /* PDU 32 (0x20): 冷却风扇阀门号 */
	uint16_t nStirValve;            /* PDU 33 (0x21): 搅拌阀门号 */
	uint32_t nCombiValve;           /* PDU 34-35 (0x22-0x23): 组合阀掩码 */
	uint16_t _pad_36_42[7];         /* PDU 36-42: 保留 */
	uint16_t nPoolLdCur;            /* PDU 43 (0x2B): 比色皿光源电流 */
	uint16_t nLL1LdCur;             /* PDU 44 (0x2C): 液位1光源电流 */
	uint16_t nLL2LdCur;             /* PDU 45 (0x2D): 液位2光源电流 */
	uint16_t nBlankPipeLowTHR;      /* PDU 46 (0x2E): 空白管路低阈值 */
	uint16_t nBlankPipeUpTHR;       /* PDU 47 (0x2F): 空白管路上阈值 */
	uint16_t nTotalRefTHR;          /* PDU 48 (0x30): 全反射阈值 */
	int16_t  nStepperMotorSpeed;    /* PDU 49 (0x31): 步进电机速度 (×0.1rpm) */
	uint16_t _pad_50_54[5];         /* PDU 50-54: 保留 */
	uint16_t nCombiValveL;          /* PDU 55 (0x37): 组合阀低位 */
	uint16_t nCombiValveH;          /* PDU 56 (0x38): 组合阀高位 */
	uint16_t _pad_57_106[50];       /* PDU 57-106: 保留 */
	uint16_t RW50;                  /* PDU 107 (0x6B): 工作模式 */
	uint16_t _pad_108_115[8];       /* PDU 108-115: 保留 */
	uint16_t RW4229;                /* PDU 116 (0x74): 当前步骤 */
	uint16_t RW4230;                /* PDU 117 (0x75): 报警/错误码 */
	uint16_t _pad_118_129[12];      /* PDU 118-129: 保留 */
	uint16_t nAutoRun;              /* PDU 130 (0x82): 自动运行触发 */
	uint16_t nResetRun;             /* PDU 131 (0x83): 复位运行 */
	uint16_t _pad_132_147[16];      /* PDU 132-147: 保留 */
	uint16_t nStartMode;            /* PDU 148 (0x94): 启动模式 */
	uint16_t _pad_149;              /* PDU 149: 保留 */
	uint16_t nControlledStep;       /* PDU 150 (0x96): 受控步骤 */
	uint16_t _pad_151_166[16];      /* PDU 151-166: 保留 */
	uint16_t nCMDTranMoni;          /* PDU 167 (0xA7): 命令传输监视 */
	uint16_t _pad_168_197[30];      /* PDU 168-197: 保留 */
	uint16_t nTestTemper;           /* PDU 198 (0xC6): 测试温度 */
	uint16_t nTestTemperHoldtime;   /* PDU 199 (0xC7): 测试温度保持时间 */
	uint16_t _pad_200_247[48];      /* PDU 200-247: 保留 */
	uint16_t nHMILeftTime;          /* PDU 248 (0xF8): HMI 剩余时间 */
	uint16_t nPrecTotalTime;        /* PDU 249 (0xF9): 流程总时间 */
	/* === AutoRun 扩展字段 === */
	uint16_t RW80;                  /* PDU 250 (0xFA): 浊度补偿 V3 电压 */
	uint16_t RW81;                  /* PDU 251 (0xFB): 反应参比 V2 电压 */
	uint16_t RW82;                  /* PDU 252 (0xFC): 蒸馏水/空白 V1 电压 */
	float    RW125;                 /* PDU 253-254 (0xFD): 浊度补偿系数 (0=关闭) */
	float    fAbsorbanc;            /* PDU 255-256 (0xFF): 吸光度 log10(V1/V2) */
	float    fK;                    /* PDU 257-258 (0x101): 标定斜率 k */
	float    fB;                    /* PDU 259-260 (0x103): 标定截距 b */
	float    fConcentrationVale;    /* PDU 261-262 (0x105): 浓度计算值 */
	uint16_t nMeasureComplet;       /* PDU 263 (0x107): 测量已完成标志 */
	uint16_t nZeroComplet;          /* PDU 264 (0x108): 零标已完成标志 */
	uint16_t nUpdateData;           /* PDU 265 (0x109): 数据更新标志 */
} MODBUS_HOLDING_REG_t;
#pragma pack(pop)

/* ==================== 输入寄存器结构体 (只读, 3X) ==================== */

#pragma pack(push, 1)
typedef struct {
	uint16_t _pad_0;                /* 0: 保留 */
	uint16_t nFirmVersion;          /* 1: 固件版本 */
	uint16_t nSysErrorCode;         /* 2: 系统错误码 */
	uint16_t nDevBeRunning;         /* 3: 设备运行状态 */
	uint16_t _pad_4_14[11];         /* 4-14: 保留 */
	uint16_t nLL1AD;                /* 15: 低液位 AD */
	uint16_t nLL2AD;                /* 16: 高液位 AD */
	uint16_t _pad_17_21[5];         /* 17-21: 保留 */
	uint16_t nStepCode;             /* 22: 步骤码 */
	uint16_t _pad_23_43[21];        /* 23-43: 保留 */
	uint16_t nFunErrorCode;         /* 44: 功能错误码 */
} MODBUS_INPUT_REG_t;
#pragma pack(pop)

/* ==================== MB3 专用寄存器 (USART3 管控仪) ==================== */

#define MB3_HOLDING_START  100
#define MB3_HOLDING_NREGS  8
#define MB3_INPUT_START    200
#define MB3_INPUT_NREGS    202

#pragma pack(push, 1)
typedef struct {
	uint16_t _pad[100];             /* 0-99: 保留 */
	uint16_t RecriminateYear;       /* 100 */
	uint16_t RecriminateMonth;      /* 101 */
	uint16_t RecriminateDay;        /* 102 */
	uint16_t RecriminateHours;      /* 103 */
	uint16_t RecriminateMin;        /* 104 */
	uint16_t RecriminateSec;        /* 105 */
	uint16_t RecriminateCMD;        /* 106: 0=无 1=零点校准 2=量程校准 3=标液核查 */
} MODBUS3_HOLDING_REG_t;

typedef struct {
	uint16_t _pad[200];             /* 0-199: 保留 */
	uint16_t DeviceType;            /* 200 */
	uint16_t SamplingYear;          /* 201 */
	uint16_t SamplingMonth;         /* 202 */
	uint16_t SamplingDay;           /* 203 */
	uint16_t SamplingHours;         /* 204 */
	uint16_t SamplingMin;           /* 205 */
	uint16_t SamplingSec;           /* 206 */
	uint16_t CompletedYear;         /* 207 */
	uint16_t CompletedMonth;        /* 208 */
	uint16_t CompletedDay;          /* 209 */
	uint16_t CompletedHours;        /* 210 */
	uint16_t CompletedMin;          /* 211 */
	uint16_t CompletedSec;          /* 212 */
	uint16_t TestDataL;             /* 213: float 低字 */
	uint16_t TestDataH;             /* 214: float 高字 */
	uint16_t TestAborbanL;          /* 215 */
	uint16_t TestAborbanH;          /* 216 */
	uint16_t TestSampleFlag;        /* 217 */
	uint16_t TestResultFlag;        /* 218 */
	uint16_t _pad_219_249[31];      /* 219-249: 保留 */
	uint16_t MainState;             /* 250: 0=待机 1=测量 2=校准 3=核查 4=维护 5=故障 */
	uint16_t SubState;              /* 251 */
	uint16_t _pad_252_299[48];      /* 252-299 */
	uint16_t MeasurementCycleL;     /* 300: float */
	uint16_t MeasurementCycleH;     /* 301 */
	uint16_t _pad_302_305[4];       /* 302-305 */
	uint16_t kL;                    /* 306: 斜率 k float 低字 */
	uint16_t kH;                    /* 307: 斜率 k float 高字 */
	uint16_t bL;                    /* 308: 截距 b float 低字 */
	uint16_t bH;                    /* 309: 截距 b float 高字 */
	uint16_t _pad_310_311[2];       /* 310-311 */
	uint16_t ZerocalibrationYear;   /* 312 */
	uint16_t ZerocalibrationMonth;  /* 313 */
	uint16_t ZerocalibrationDay;    /* 314 */
	uint16_t ZerocalibrationHours;  /* 315 */
	uint16_t ZerocalibrationMin;    /* 316 */
	uint16_t ZerocalibrationSec;    /* 317 */
	uint16_t ZerocalibConcentrationL; /* 318: float */
	uint16_t ZerocalibConcentrationH; /* 319 */
	uint16_t ZerocalibValueL;       /* 320 */
	uint16_t ZerocalibValueH;       /* 321 */
	uint16_t ZerocalibAborbanL;     /* 322 */
	uint16_t ZerocalibAborbanH;     /* 323 */
	uint16_t _pad_324_329[6];       /* 324-329 */
	uint16_t RangecalibrationYear;  /* 330 */
	uint16_t RangecalibrationMonth; /* 331 */
	uint16_t RangecalibrationDay;   /* 332 */
	uint16_t RangecalibrationHours; /* 333 */
	uint16_t RangecalibrationMin;   /* 334 */
	uint16_t RangecalibrationSec;   /* 335 */
	uint16_t _pad_336_363[28];      /* 336-363 */
	uint16_t StandardcheckYear;     /* 364 */
	uint16_t StandardcheckMonth;    /* 365 */
	uint16_t StandardcheckDay;      /* 366 */
	uint16_t StandardcheckHours;    /* 367 */
	uint16_t StandardcheckMin;      /* 368 */
	uint16_t StandardcheckSec;      /* 369 */
} MODBUS3_INPUT_REG_t;
#pragma pack(pop)

/* ==================== 全局数据指针 ==================== */
extern MODBUS_HOLDING_REG_t *pMbHoldData;
extern MODBUS_INPUT_REG_t   *pMbInputData;
extern uint8_t              *pMbCoilData;

extern MODBUS3_HOLDING_REG_t *pMb3HoldData;
extern MODBUS3_INPUT_REG_t   *pMb3InputData;

extern volatile bool HaveNewCMDFlag;
extern volatile uint8_t g_mb_inst;
extern bool AnalyzerBeWorking;  /* 命令执行互斥标志: true=HostCmdProcess 正在执行 */

/* ==================== Modbus 任务线程入口 ==================== */
void task_modbus_usart1(void *, void *, void *);
void task_modbus_usart3(void *, void *, void *);
void task_modbus_usart6(void *, void *, void *);

/* ==================== RS232 命令处理 ==================== */
void RS232CmdProcess(void);

/* ==================== 阀门状态更新 ==================== */
void UpdataValveState(uint8_t *pCoilBuf);

#endif /* TASK_MODBUS_H */
