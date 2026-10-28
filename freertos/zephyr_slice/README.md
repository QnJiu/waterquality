# Zephyr ADS1220 / Modbus 切片

此示例基于 Zephyr 3.7 API，复用 `../drivers/ads1220.c`、`../drivers/adc_reader.c` 和 `../app/adc_status.c`。SPI 配置为 mode 1，默认 MUX 8、增益 1、内部 2.048 V 参考，单次转换每秒一次；这些参数是示例，不代表已校准的实物。

按选定板卡将 `app.overlay.example` 复制为 `app.overlay` 并补充 SPI、UART 的 pinctrl 及 RS485 DE 引脚；示例 GPIO 与 F407 参考驱动一致，但板级复用、时钟及接线仍需核对。设备树别名 `tp-adc` 指向 SPI 子节点，`tp-modbus` 指向 `zephyr,modbus-serial` 子节点。调试日志不能与 Modbus 共用 UART。

安装 Zephyr 3.7 及对应工具链后，运行 `west build -b <board> freertos/zephyr_slice`。原生模拟构建可使用主机工具链：设置 `ZEPHYR_TOOLCHAIN_VARIANT=host`，再运行 `west build -b native_sim/native/64 freertos/zephyr_slice`。WSL 中以 Zephyr 3.7、GCC 15 和 CMake 4.4 验证过编译和启动；CMake 4.4 构建时还需附加 `-- -DCMAKE_POLICY_VERSION_MINIMUM=3.5`。

原生模拟配置使用 Zephyr SPI、GPIO 和 UART 模拟器；`src/ads1220_emul.c` 只模拟本示例所用的命令和固定 ADC 原始值。端到端自测通过模拟串口发送 Modbus 读请求，校验采样状态、固定原始值转换得到的 256000 微伏和响应 CRC。主机侧 `test_adc_status` 使用同一个 `tests/adc_slice_vector.h` 输入和期望值，检查共享采集核心及 Modbus 响应；另提供 `sh freertos/examples/posix_slice/run.sh`，在 Linux/WSL 的 FreeRTOS POSIX 调度器中运行同一向量，校验采集与 Modbus 响应；该示例使用模拟 SPI、直接调用协议处理，不含串口硬件。两侧均不是目标板测试。

从站地址 1，19200 baud、偶校验；保持寄存器地址 0–5 依次为状态、`tp_result`、通道、序号、有符号微伏高字和低字，错误时读数清零。Zephyr 的服务端逐寄存器调用回调，采样可能在同一请求的回调间更新；读取端应重复读取完整寄存器组并确认两次相同后使用。失败重试和通信时序仍需在具体板卡验证。仅提供只读寄存器，写操作不支持。

运行端到端自测（使用独立构建目录）：

```sh
west build -b native_sim/native/64 freertos/zephyr_slice -d build-tp-selftest -- -DEXTRA_CONF_FILE=tests/native_selftest.conf
./build-tp-selftest/zephyr/zephyr.exe
```

成功时输出 `TP_NATIVE_SELFTEST PASS` 并以状态码 0 退出。CMake 4.4 环境还需在 `--` 后追加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`。