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
.\build\tp_simulator.exe calibration
ctest --test-dir build --output-on-failure
```

正常示例最后显示 `result_valid=1`、`safe=1`；`interlock` 模拟互锁故障，最后显示 `result_valid=0`、`safe=1`。`periodic` 和 `daily` 分别通过 Modbus 配置周期与每日定时，在模拟时间到 60 秒时自动启动完整流程；每日模式使用模拟 RTC。`recover` 模拟互锁故障后，先尝试清故障、恢复输入，再清故障并重新完成检测；`sensor` 模拟传感器故障；`recover-sensor` 演示传感器问题未排除时恢复失败，排除后发送 Modbus 命令 11 恢复 ADC、自检并清故障，再重新测量；`stop-rinse` 模拟清洗时停机。`safe=1` 表示模拟器检查到输出进入安全状态；模拟值不代表实际仪器的测量精度。

## 2. 按顺序阅读

1. `examples/simulator.c`：模拟配方、设备输入和 Modbus 命令，观察一次流程。
2. `app/controller.c` 和 `include/tp_core.h`：流程状态、定时及安全停止。
3. `app/measurement.c`：测量和温控 PID 计算。
4. `protocol/` 和 `docs/MODBUS.md`：Modbus 通信及寄存器。
5. `firmware/`、`firmware_f030/`：主控与从控参考固件，具体构建选项分别见目录内的 README。

F407 与 F030 的教学装配可通过 F407 构建选项 `TP_F407_TEACHING_SUBORDINATE=ON` 启用，具体步骤见 `firmware/README.md`。从控状态未知、过期或报告故障时，主控会阻止启动或停止运行；F030 也需启用对应参考输入。配方和接线都是示例，不能用于实际测量。

不同硬件的管脚、传感器、反应参数和标定数据需要按实物配置。默认 F407 固件是心跳示例，业务入口需单独配置；F030 浅睡眠和 RTC 定时 STOP 均为可选，尚未实机验证。ESP32 局域网升级网关见 `examples/esp32_gateway/README.md`；阿里云状态上报和可选升级示例见 `examples/esp32_cloud/README.md`，均需实际设备联调。软件实现和实机验证的详细状态见 `docs/STATUS.md`。

运行 `calibration` 可观察两种模拟标准液的自动采样、两点标定结果应用和标液核查。示例通过 Modbus 命令 6、7、9 依次启动、应用和核查；应用只改变内存，示例不执行保存。标准液读数和浓度是教学数据，实际配方需按仪器确定。

已接入串口的设备运行检测时，可用 `python tools/tp_modbus.py --port COM3 temperature` 查看最近一次温度采样；未运行或尚未采样时显示 null。

每日定时模式需要先设置 RTC。可用 `python tools/tp_modbus.py --port COM3 set-clock <UTC秒数>` 设置，然后用 `clock-status` 查询结果；具体地址见 `docs/MODBUS.md`；校时只允许在设备未运行时操作。

## 3. 故障后如何操作

在已配置的设备上，先查看 Modbus 状态与自检结果（寄存器 10、11、19），排除互锁、液位、漏液或传感器问题。设备已配置 ADC 恢复和自检时，可在故障状态下执行 `python tools/tp_modbus.py --port COM3 command recover`，或点击上位机“故障恢复”：设备先关闭输出，再恢复 ADC、执行自检并清除故障；任一步失败都会保留故障。也可分别执行命令 8 恢复 ADC、命令 3 清除故障；清故障会重新检查已配置的自检条件；传感器或超温故障还要求温度读数有效且低于上限。温度检查不能代替 ADC、液位等其他自检。故障未排除时仍会拒绝启动。恢复命令不会直接继续中断的测量；受控模式需重新启动，周期或定时模式可在下一个计划时间自动启动。旧结果不会作为有效结果使用。命令和完整寄存器说明见 `docs/MODBUS.md`。

以上操作不能替代硬件检查：实际故障原因、接线和安全输出必须结合板卡确认。电脑模拟只验证软件流程，不证明实物可靠性或测量精度。
