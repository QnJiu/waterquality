# Zephyr ADS1220 / Modbus 切片

此示例基于 Zephyr 3.7 API，复用 `../drivers/ads1220.c`、`../drivers/adc_reader.c` 和 `../app/adc_status.c`。SPI 配置为 mode 1，默认 MUX 8、增益 1、内部 2.048 V 参考，单次转换每秒一次；这些参数是示例，不代表已校准的实物。

按选定板卡将 `app.overlay.example` 复制为 `app.overlay` 并补充 SPI、UART 的 pinctrl 及 RS485 DE 引脚；示例 GPIO 与 F407 参考驱动一致，但板级复用、时钟及接线仍需核对。设备树别名 `tp-adc` 指向 SPI 子节点，`tp-modbus` 指向 `zephyr,modbus-serial` 子节点。调试日志不能与 Modbus 共用 UART。

安装 Zephyr 3.7 及工具链后，运行 `west build -b <board> freertos/zephyr_slice`。当前开发环境尚无 Zephyr SDK，未执行该构建。SPI/DRDY 适配在主机上用替身测试，不等同于 Zephyr 编译或硬件测试。

从站地址 1，19200 baud、偶校验；保持寄存器地址 0–5 依次为状态、`tp_result`、通道、序号、有符号微伏高字和低字，错误时读数清零。Zephyr 的服务端逐寄存器调用回调，采样可能在同一请求的回调间更新；读取端应重复读取完整寄存器组并确认两次相同后使用。失败重试和通信时序仍需在具体板卡验证。仅提供只读寄存器，写操作不支持。
