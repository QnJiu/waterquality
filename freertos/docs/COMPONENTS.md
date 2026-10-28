# 教学参考器件与代码入口

这份清单记录当前代码采用的参考型号，方便阅读和后续选型；它不是已经验证的实物 BOM。默认 F407 固件只是心跳演示，完整业务入口和部分器件需要选择构建选项。模拟与交叉编译不能确认电气连接或性能。

| 用途 | 参考型号或方案 | 代码入口 | 使用情况 |
|---|---|---|---|
| 主控 | STM32F407VG，1 MiB Flash | `firmware/` | FreeRTOS 业务入口可选；升级分区按该容量设计 |
| 模拟信号采集 | ADS1220，SPI、DRDY | `drivers/ads1220.c` | 业务入口使用；光学传感器、前端及通道换算需另行确定 |
| 反应池温度 | 10 kΩ B3950 NTC，10 kΩ 上拉 | `drivers/ntc.c`、`firmware/tp_f407_temperature.c` | 参考 ADC1 接口；探头曲线和精度需核实 |
| 时钟 | DS3231，I²C | `drivers/ds3231.c` | 用于每日定时和时间记录；后备电源需按实际配置 |
| 配置和事件日志 | MB85RC16，2 KiB FRAM，I²C | `drivers/mb85rc16.c`、`storage/` | 参考业务装配使用；升级元数据也可使用独立 FRAM |
| 备用配置存储 | M24C02 EEPROM，I²C | `drivers/m24c02.c`、`drivers/m24c02_store.c` | 可选配置存储方案；不是默认业务日志器件 |
| 升级暂存 | W25Q128 外部 Flash，SPI | `drivers/w25q128.c`、`firmware/tp_f407_stage_reference.c` | 可选暂存方案；双镜像还依赖 F407 内部 Flash 布局 |
| 从控与门禁 | STM32F030C8、PN532 I²C | `firmware_f030/`、`drivers/pn532.c` | 可选从控及门禁；示例 UID 白名单不具备防复制认证 |
| 联网 | ESP32，MicroPython、Wi-Fi | `examples/esp32_cloud/` | 可选云端上报和升级示例；需支持所用 TLS 和串口接口 |
| 泵、阀、加热器 | STEP/DIR 泵驱动、阀门驱动、PWM 加热驱动 | `firmware/tp_f407_pump.c`、`firmware/tp_f407_valves.c`、`firmware/tp_f407_heater.c` | 接口及参考输出已有实现；驱动器、电源、负载和安全态需确定 |
| 液位、漏液、液滴和试剂状态 | GPIO/EXTI 数字输入 | `firmware_f030/tp_f030_reference_inputs.c` | 参考输入电平；传感器型号、滤波和电路需确定 |
| 上位机通信 | UART、RS485 或 RS232 收发器 | `protocol/`、`firmware/` | 收发器型号、电平及总线接线需确定 |

业务 FRAM 与升级 FRAM 地址可能冲突；同时使用时参考装配采用不同 I²C 总线，见 [F407 说明](../firmware/README.md)。引脚、电压、上拉、电源、接口和板级配置以同一文档及 [F030 说明](../firmware_f030/README.md) 为起点，再结合实物核对。真实化学配方、光学前端和泵量不能仅凭型号确定。
