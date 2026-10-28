# STM32F407 FreeRTOS 启动骨架

这是独立的交叉编译目标，暂以 STM32F407VG（1 MiB Flash、128 KiB 主 SRAM）为参考芯片；引脚、外设、外部晶振和检测配方均未配置。时钟采用复位后的 16 MHz HSI，CMSIS 启动代码初始化 FPU，FreeRTOS 使用 Cortex-M4F 硬浮点端口。固件只创建每秒递增 `tp_scheduler_heartbeat` 的任务，调试器可观察该变量。它不启动检测业务，不操作泵、阀或加热器；断言只关中断并停机，尚无执行器安全关断，不可用于控制真实仪器。

需要 CMake、Ninja 和 arm-none-eabi-gcc（本机使用 GCC 16.1.0）。在本目录执行：

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/arm-gcc.cmake"
cmake --build build
```

输出 `tp_f407.elf`、`tp_f407.hex` 和 `tp_f407.map`。链接使用 nosys，未提供标准 I/O 系统调用，工具链可能对此发出警告。编译包含控制核心和 RTOS 适配代码以检查目标架构兼容性，但由于当前主程序没有引用它们，链接器会移除未引用段；不代表业务链路已集成。必须核对实物型号、Flash 大小、引脚、电气安全态和保护电路后才能实现板级对接；此链接布局尚未划分 Bootloader 或 OTA 区域。实机尚未验证。

依赖来源：FreeRTOS Kernel V10.5.1（随源码 MIT 许可）；CMSIS 5.9.0 Core（Apache-2.0）；ST CMSIS Device F4 v2.6.10 与 STM32F4 HAL/LL 头文件 v1.8.5（许可证各随目录保存）。项目目前只引入 LL 头文件，尚无具体 LL 外设驱动实现。

## 参考存储总线

`tp_f407_i2c.c` 提供 I2C2 到 `tp_i2c_io` 的适配，假设单主机、PB10=SCL、PB11=SDA（AF4）、外部上拉、APB1 16 MHz、100 kHz，目标器件为 MB85RC16。此引脚没有出现在旧设备树的阀门或 USART3 分配中，仍须按实际原理图核对。初始化前调用 `SystemCoreClockUpdate()`；驱动只接受 HSI 16 MHz 且 AHB/APB1 不分频的配置。I2C 操作限时，错误后重置外设；物理总线卡死需要硬件恢复，驱动不会自动输出九个恢复脉冲。事务在调度器启动前单线程执行，调度后使用互斥锁；只在低优先级存储任务调用。

连接顺序：`tp_f407_i2c_init` → `tp_f407_i2c_io` → `tp_mb85rc16_init`（存储区起始 256 字节对齐）→ `tp_mb85rc16_store` → `tp_runtime_bootstrap_storage`。各对象及其上下文须长期有效。当前心跳主程序不调用这条链路；I2C 时序、上拉、电气参数、总线错误恢复和掉电保存都尚未在目标板验证。

## ADS1220 SPI2 参考接口

`tp_f407_ads1220_spi` 假定系统和 APB1 均为 16 MHz，使用 SPI2 模式 1、1 MHz，PB13 SCK、PB14 MISO、PB15 MOSI、PB12 软件片选，PE14 上拉输入作为低有效 DRDY。初始化必须在调度器启动前执行一次，设备实例持续有效；接口不可从中断调用。传输竞争时立即返回忙，单次事务上限 5 字节，循环等待设有 1 ms 周期计数期限（不限制抢占时间），错误时释放片选并重置 SPI。该设备需与 SPI2 的其他使用者共享同一互斥策略。

本接口和通用 ADS1220 驱动已列入交叉编译，但尚未由心跳主程序初始化，链接垃圾回收可能移除它们；引脚电气连接、时序和模拟采样均未在硬件上验证。
