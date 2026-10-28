# 学生快速上手

先在电脑上运行模拟器，再阅读业务状态机；接板卡之前不要使用示例配方进行真实测量。

## 1. 编译和运行

在本目录的 PowerShell 中执行，电脑需要已安装 CMake、Ninja 和 C 编译器：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\tp_simulator.exe
.\build\tp_simulator.exe interlock
ctest --test-dir build --output-on-failure
```

正常示例最后显示 `result_valid=1`、`safe=1`；`interlock` 模拟互锁故障，最后显示 `result_valid=0`、`safe=1`。`sensor` 模拟传感器故障，`stop-rinse` 模拟清洗时停机。`safe=1` 表示模拟器检查到输出进入安全状态；模拟值不代表实际仪器的测量精度。

## 2. 按顺序阅读

1. `examples/simulator.c`：模拟配方、设备输入和 Modbus 命令，观察一次流程。
2. `app/controller.c` 和 `include/tp_core.h`：流程状态、定时及安全停止。
3. `app/measurement.c`：测量和温控 PID 计算。
4. `protocol/` 和 `docs/MODBUS.md`：Modbus 通信及寄存器。
5. `firmware/`、`firmware_f030/`：主控与从控参考固件，具体构建选项分别见目录内的 README。

不同硬件的管脚、传感器、反应参数和标定数据需要按实物配置。默认 F407 固件是心跳示例，业务入口需单独配置；F030 浅睡眠可选，STOP 尚未接入。ESP32 局域网升级网关见 `examples/esp32_gateway/README.md`；阿里云状态上报和可选升级示例见 `examples/esp32_cloud/README.md`，均需实际设备联调。软件实现和实机验证的详细状态见 `docs/STATUS.md`。

## 3. 故障后如何操作

在已配置的设备上，先查看 Modbus 状态与自检结果（寄存器 10、11、19），排除互锁、液位、漏液或传感器问题。ADC 配置失效时可先执行命令 8 恢复 ADC，再执行命令 3 清除故障；清故障会重新检查已配置的自检条件。故障未排除时仍会拒绝启动。恢复后需重新启动测量，旧结果不会作为有效结果使用。命令和完整寄存器说明见 `docs/MODBUS.md`。

以上操作不能替代硬件检查：实际故障原因、接线和安全输出必须结合板卡确认。电脑模拟只验证软件流程，不证明实物可靠性或测量精度。
