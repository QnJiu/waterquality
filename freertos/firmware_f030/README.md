# STM32F030 参考启动骨架

本目标假定 STM32F030C8（64 KiB Flash、8 KiB SRAM），使用复位后的 8 MHz HSI 和 FreeRTOS Cortex-M0 端口。仅创建心跳任务，调试器可观察 `tp_f030_heartbeat`；未初始化任何业务外设、门锁或低功耗功能。芯片型号、引脚、电源和连接须结合实物确认，尚未实机验证。

依赖现有 FreeRTOS Kernel V10.5.1、CMSIS Core 5.9.0；STM32F0 CMSIS 设备文件来自 ST 的 cmsis-device-f0 v2.3.6，提交 `8b86197c7ab77ad029c34dc73cf6d4a1e12327d8`，许可随文件保存。参考链接脚本未划分升级空间；3 KiB FreeRTOS 堆和至少 1 KiB 主栈是启动阶段的预算，实际业务接入后须重新测量栈及 RAM。

在此目录执行：

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/../firmware/arm-gcc.cmake"
cmake --build build
```

生成 ELF、HEX 和 map。硬件复位期间及异常状态下的锁输出安全态必须由电路与后续板级实现保证。
