# STM32F407 FreeRTOS 启动骨架

这是独立的交叉编译目标，暂以 STM32F407VG（1 MiB Flash、128 KiB 主 SRAM）为参考芯片；引脚、外设、外部晶振和检测配方均未配置。时钟采用复位后的 16 MHz HSI，CMSIS 启动代码初始化 FPU，FreeRTOS 使用 Cortex-M4F 硬浮点端口。固件只创建每秒递增 `tp_scheduler_heartbeat` 的任务，调试器可观察该变量。它不启动检测业务，不操作泵、阀或加热器；断言只关中断并停机，尚无执行器安全关断，不可用于控制真实仪器。

需要 CMake、Ninja 和 arm-none-eabi-gcc（本机使用 GCC 16.1.0）。在本目录执行：

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/arm-gcc.cmake"
cmake --build build
```

输出 `tp_f407.elf`、`tp_f407.hex` 和 `tp_f407.map`。链接使用 nosys，未提供标准 I/O 系统调用，工具链可能对此发出警告。编译包含控制核心和 RTOS 适配代码以检查目标架构兼容性，但由于当前主程序没有引用它们，链接器会移除未引用段；不代表业务链路已集成。必须核对实物型号、Flash 大小、引脚、电气安全态和保护电路后才能实现板级对接；此链接布局尚未划分 Bootloader 或 OTA 区域。实机尚未验证。

依赖来源：FreeRTOS Kernel V10.5.1（随源码 MIT 许可）；CMSIS 5.9.0 Core（Apache-2.0）；ST CMSIS Device F4 v2.6.10 与 STM32F4 HAL/LL 头文件 v1.8.5（许可证各随目录保存）。项目目前只引入 LL 头文件，尚无具体 LL 外设驱动实现。
