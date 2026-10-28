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

不同硬件的管脚、传感器、反应参数和标定数据需要按实物配置。默认 F407 固件是心跳示例，业务入口需单独配置；F030 浅睡眠可选，STOP 尚未接入。ESP32 升级网关见 `examples/esp32_gateway/README.md`，它是局域网教学示例，尚未实现阿里云设备协议。软件实现和实机验证的详细状态见 `docs/STATUS.md`。
