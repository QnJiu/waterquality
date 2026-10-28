# 学生快速上手

先在电脑上运行模拟器，再阅读业务状态机；接板卡之前不要使用示例配方进行真实测量。

## 1. 编译和运行

在本目录的 PowerShell 中执行，电脑需要已安装 CMake、Ninja 和 C 编译器：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\tp_simulator.exe
.\build\tp_simulator.exe interlock
.\build\tp_simulator.exe recover
.\build\tp_simulator.exe recover-sensor
.\build\tp_simulator.exe periodic
.\build\tp_simulator.exe daily
.\build\tp_simulator.exe controlled
.\build\tp_simulator.exe calibration
ctest --test-dir build --output-on-failure
```

正常示例最后显示 `result_valid=1`、`safe=1`；`interlock` 模拟互锁故障，最后显示 `result_valid=0`、`safe=1`。`controlled` 展示受控模式每一步完成后等待，并模拟发送 Modbus 命令 5 继续；`periodic` 和 `daily` 分别通过 Modbus 配置周期与每日定时，在模拟时间到 60 秒时自动启动完整流程；周期示例继续运行到 120 秒，再完成第二次检测，每日模式使用模拟 RTC。`recover` 模拟互锁故障后，先尝试清故障、恢复输入，再清故障并重新完成检测；`sensor` 模拟传感器故障；`recover-sensor` 演示传感器问题未排除时恢复失败，排除后发送 Modbus 命令 11 恢复 ADC、自检并清故障，再重新测量；`stop-rinse` 模拟清洗时停机。`safe=1` 表示模拟器检查到输出进入安全状态；模拟值不代表实际仪器的测量精度。

## Windows 交叉编译路径

某些 Windows 版 `arm-none-eabi-gcc` 在项目路径含中文时无法链接。可在 PowerShell 中为项目目录临时映射一个空闲盘符，并通过该盘符配置和编译；例如 W 盘未使用时：

```powershell
subst W: "E:\ai_dir\project_enbeded\水质检测"
cmake -S W:/freertos/firmware -B W:/freertos/build-f407-example -G Ninja -DCMAKE_TOOLCHAIN_FILE=W:/freertos/firmware/arm-gcc.cmake
cmake --build W:/freertos/build-f407-example
```

以上是默认心跳固件。业务配置及 F030 构建选项分别见 `firmware/README.md` 和 `firmware_f030/README.md`；配置、构建和指定源文件时均使用同一英文盘符。完成后可用 `subst W: /D` 解除映射；再次构建这个目录时需要重新映射相同盘符，或重新配置构建目录。映射不复制项目文件，先确认 W 盘未被占用。
## 2. 按顺序阅读

1. `examples/simulator.c`：模拟配方、设备输入和 Modbus 命令，观察一次流程。
2. `app/controller.c` 和 `include/tp_core.h`：流程状态、定时及安全停止。
3. `app/measurement.c`：测量和温控 PID 计算。
4. `protocol/` 和 `docs/MODBUS.md`：Modbus 通信及寄存器。
5. `firmware/`、`firmware_f030/`：主控与从控参考固件，具体构建选项分别见目录内的 README。

### 功能与代码位置

| 想了解的功能 | 从这里开始 |
|---|---|
| 启动与任务 | `firmware/main_business.c`、`firmware/tp_f407_teaching_config.c`、`os/freertos_runtime.c` |
| 检测步骤、安全停止与自检 | `app/controller.c`、`app/service.c`、`app/preflight.c` |
| 测量、温控与标定 | `app/measurement.c`、`app/auto_calibration.c`，ADC 驱动见 `drivers/` |
| 周期、每日定时和受控模式 | `app/schedule.c`、`app/service.c` |
| 寄存器和上位机 | `protocol/`、`docs/MODBUS.md`、`tools/tp_modbus.py`、`tools/tp_gui.py` |
| 参数和事件保存 | `storage/config_store.c`、`storage/event_log.c` |
| 升级与从控 | `bootloader/`、`firmware_boot/`、`examples/esp32_cloud/`、`firmware_f030/` |
| 两种 RTOS 的采集示例 | `examples/posix_slice/`、`examples/posix_control/`、`zephyr_slice/` |

默认 F407 入口仅运行心跳；上表业务入口需要按 `firmware/README.md` 的选项单独构建。电脑模拟器和主机测试主要验证业务及协议逻辑；POSIX 示例用真实 FreeRTOS 调度器验证一条模拟 ADC 到 Modbus 的链路，并与 Zephyr 模拟结果对照。`examples/posix_control/run.sh` 还通过控制任务和 Modbus 命令逐步完成一次模拟采样、计算并读取结果，再启动一次并验证停止时阀门关闭；还用内存模拟存储验证后台日志写入和查询、参数保存及擦除失败保护（需 Linux 或 WSL）。完整业务调度、硬件和云链路还需要进一步验证。可选资源诊断可观察栈、堆及估算的 CPU 忙碌比例，实际板上尚未测量。

F407 与 F030 的教学装配可通过 F407 构建选项 `TP_F407_TEACHING_SUBORDINATE=ON` 启用，具体步骤见 `firmware/README.md`。从控状态未知、过期或报告故障时，主控会阻止启动或停止运行；F030 也需启用对应参考输入。配方和接线都是示例，不能用于实际测量。

不同硬件的管脚、传感器、反应参数和标定数据需要按实物配置。默认 F407 固件是心跳示例，业务入口需单独配置；F030 浅睡眠和 RTC 定时 STOP 均为可选，尚未实机验证。ESP32 局域网升级网关见 `examples/esp32_gateway/README.md`；阿里云状态上报和可选升级示例见 `examples/esp32_cloud/README.md`，均需实际设备联调。软件实现和实机验证的详细状态见 `docs/STATUS.md`。

运行 `calibration` 可观察两种模拟标准液的自动采样、两点标定结果应用和标液核查。示例通过 Modbus 命令 6、7、9 依次启动、应用和核查；应用只改变内存，示例不执行保存。标准液读数和浓度是教学数据，实际配方需按仪器确定。

已接入串口的设备运行检测时，可用 `python tools/tp_modbus.py --port COM3 temperature` 查看最近一次温度采样；未运行或尚未采样时显示 null。

每日定时模式需要先设置 RTC。可用 `python tools/tp_modbus.py --port COM3 set-clock <UTC秒数>` 设置，然后用 `clock-status` 查询结果；具体地址见 `docs/MODBUS.md`；校时只允许在设备未运行时操作。

## 3. 故障后如何操作

在已配置的设备上，先查看 Modbus 状态与自检结果（寄存器 10、11、19），排除互锁、液位、漏液或传感器问题。设备已配置 ADC 恢复和自检时，可在故障状态下执行 `python tools/tp_modbus.py --port COM3 command recover`，或点击上位机“故障恢复”：设备先关闭输出，再恢复 ADC、执行自检并清除故障；任一步失败都会保留故障。也可分别执行命令 8 恢复 ADC、命令 3 清除故障；清故障会重新检查已配置的自检条件；传感器或超温故障还要求温度读数有效且低于上限。温度检查不能代替 ADC、液位等其他自检。故障未排除时仍会拒绝启动。恢复命令不会直接继续中断的测量；受控模式需重新启动，周期或定时模式可在下一个计划时间自动启动。旧结果不会作为有效结果使用。命令和完整寄存器说明见 `docs/MODBUS.md`。

以上操作不能替代硬件检查：实际故障原因、接线和安全输出必须结合板卡确认。电脑模拟只验证软件流程，不证明实物可靠性或测量精度。

## 4. 两种 RTOS 的采集示例

Linux 或 WSL 下运行 `sh freertos/examples/posix_slice/run.sh`，可在 FreeRTOS POSIX 任务中观察模拟 ADS1220 采集与 Modbus 响应。Zephyr 对应自测和相同输入向量的说明见 `zephyr_slice/README.md`。两者使用模拟外设，不代表板卡测试。
