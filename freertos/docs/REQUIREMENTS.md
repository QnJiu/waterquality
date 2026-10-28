# 设计要求与软件验证对照

本表对应 `IMPLEMENTATION_PLAN.md`，帮助学生从需求找到代码和可复现的电脑测试。测试使用模拟输入或替身；板卡、化学方法和性能指标需另行验证。先按 [学生快速上手](LEARNING.md) 构建，在 `freertos` 目录执行 `ctest --test-dir build -R "测试名称" --output-on-failure`；完整回归去掉 `-R` 参数。

| 设计要求 | 主要代码入口 | 可运行的电脑验证（CTest 名称） | 硬件阶段重点 |
|---|---|---|---|
| FreeRTOS 任务、可中止检测与安全输出 | `os/freertos_runtime.c`、`app/controller.c` | `controlled`、`simulator_stop_rinse`、`integration` | 调度、停机响应及执行器安全态 |
| 泵、阀门、温度与 PID | `firmware/tp_f407_pump.c`、`app/controller.c`、`app/measurement.c` | `pulse_counter`、`valves`、`heater_duty`、`f407_teaching_config` | 泵量、温度响应及异常保护 |
| ADS1220 采样、滤波和浓度计算 | `drivers/ads1220.c`、`app/measurement.c` | `ads_schedule`、`sample_filter`、`integration` | 模拟前端、光学数据及测量精度 |
| 两点标定与标准液核查 | `app/auto_calibration.c`、`app/measurement.c` | `calibration`、`auto_calibration`、`service_auto_calibration` | 标准液、配方与实际参考值 |
| 周期、每日定时、受控模式和自检 | `app/schedule.c`、`app/service.c`、`app/preflight.c` | `simulator_periodic`、`simulator_daily`、`simulator_controlled`、`preflight` | RTC 保持、输入电平及流程时间 |
| Modbus RTU 和上位机 | `protocol/`、`tools/tp_gui.py` | `rtu_port`、`freemodbus_host`、`freertos_runtime_freemodbus`、`tp_gui_tool` | 多串口、RS485 接线与通信时序 |
| 配置、日志和掉电恢复 | `storage/config_store.c`、`storage/event_log.c` | `settings`、`fram`、`m24c02_store`、`event_log` | 存储器连接、复位和断电测试 |
| Bootloader、升级和回滚 | `bootloader/`、`firmware_boot/`、`examples/esp32_cloud/` | `secure_upgrade_interop`、`signed_boot`、`staged_upgrade`、`esp32_cloud_mqtt` | 分区、设备身份、可信密钥和云端联调 |
| F030、PN532、故障互锁和低功耗 | `firmware_f030/`、`app/door_service.c` | `f030_reference_inputs`、`f030_pn532_door`、`status_line_integration`、`f030_stop` | 传感器与门锁电路、休眠电流和唤醒 |
| Zephyr 采集切片 | `zephyr_slice/`、`examples/posix_slice/` | `zephyr_ads1220`；两套 RTOS 的完整示例运行见 `zephyr_slice/README.md` | 实际 SPI、DRDY 和通信时序 |

CTest 名称是入口，不表示每项设计指标均已覆盖。完整的功能约束及模拟验证范围见 [软件状态](STATUS.md)，参考型号见 [器件清单](COMPONENTS.md)。40 分钟、0–10 mg/L、±2% 和标准符合性需要真实仪器及样品验证。
