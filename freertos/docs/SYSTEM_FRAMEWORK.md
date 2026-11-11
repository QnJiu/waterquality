---
cssclasses:
  - water-quality-architecture
---

# 水质检测仪系统框架：整机架构与一次检测

阅读基线：先读 `freertos/README.md`，再依次读 `docs/LEARNING.md`、`COMPONENTS.md`、`STATUS.md`、`MODBUS.md`、`SUBORDINATE.md`，随后沿文档入口核对自有源码。未读取 build*、tests/、third_party/、.git、.vscode 中的内容，未重新编译或运行测试。

以下路径默认相对 `freertos/`；以“项目根”标注的路径相对 项目根目录。

状态标记：**实**＝软件逻辑已有实现，文档报告有主机模拟或测试；**参**＝参考适配或可选装配已有源码，实机待验证；**例**＝教学示例或局部迁移，端到端待联调。它们描述不同验证层次；“实”不表示整机已通过验收。

默认 F407 镜像仅运行心跳。以下检测链路描述选用板级配置后的业务入口；教学配置还要求启用从控，否则配置函数返回错误并安全停机。示例配方、传感器模型及标定系数均不能作为真实仪器参数或精度依据。

## 1. 整机拓扑（完整图）

本图保留 PC、ESP32、云端、F407、升级服务、ADS1220、NTC、执行器、FRAM/RTC、F030、资源输入、PN532、门锁全部 13 个模块。蓝色主控与连线突出 A 检测路径；绿色是主控外设，橙色是从控与门禁，紫色是外部接入。颜色区分职责，验证状态仍以“实／参／例”为准。

[打开完整 SVG（可放大）](assets/system-topology.svg) · [打开 PNG](assets/system-topology.png)

```mermaid
%%{init: {"theme":"base","themeVariables":{"fontFamily":"Microsoft YaHei, Arial, sans-serif","fontSize":"16px","primaryColor":"#f8fafc","primaryTextColor":"#172033","primaryBorderColor":"#94a3b8","lineColor":"#64748b","clusterBkg":"#f8fafc","clusterBorder":"#cbd5e1","edgeLabelBackground":"#ffffff"},"flowchart":{"htmlLabels":true,"curve":"linear","nodeSpacing":32,"rankSpacing":55,"padding":14,"wrappingWidth":320}}}%%
flowchart TB
    PC["例｜PC 上位机<br/>发送命令并读取设备状态与结果<br/>tools/tp_gui.py<br/>tools/tp_modbus.py"]
    ESP["例｜ESP32 网关<br/>轮询设备、上报属性并接收 OTA<br/>examples/esp32_cloud/<br/>examples/esp32_gateway/"]
    CLOUD["例｜云端对接<br/>接收属性并提供升级通知与下载地址<br/>examples/esp32_cloud/cloud.py<br/>examples/esp32_cloud/ota_notice.py"]
    CLOUD <-->|"Wi-Fi：TLS MQTT<br/>属性上报／OTA 通知；HTTPS 下载"| ESP
    M["参｜STM32F407 · FreeRTOS<br/>执行检测并装配协议、外设和安全输出<br/>firmware/main_business.c<br/>firmware/tp_f407_app.c"]
    STAGE["参｜可选升级服务<br/>校验暂存镜像、复制候选并安排重启<br/>firmware/tp_f407_stage_service.c<br/>bootloader/、firmware_boot/"]
    M <-.-|"调用：安装完成后<br/>按板级策略安全复位"| STAGE
    ADC["实＋参｜ADS1220 采集<br/>执行转换并提供光学采样信号<br/>drivers/ads1220.c<br/>firmware/tp_f407_ads1220_spi.c"]
    TEMP["参｜外部 NTC 温度<br/>提供带有效期的反应池温度<br/>drivers/ntc.c<br/>firmware/tp_f407_temperature.c"]
    OUT["参｜阀门、泵、加热器<br/>执行切阀、泵脉冲和加热 PWM<br/>firmware/tp_f407_valves.c<br/>firmware/tp_f407_pump.c<br/>firmware/tp_f407_heater.c"]
    MEM["实＋参｜FRAM 与 RTC<br/>保存配置、事件日志并提供时间<br/>drivers/mb85rc16.c<br/>drivers/ds3231.c、storage/"]
    S["参｜STM32F030 从控<br/>汇总资源故障和液滴并管理门禁<br/>firmware_f030/<br/>app/subordinate_app.c"]
    INPUT["参｜资源与液滴输入<br/>采集试剂、液位、漏液及液滴事件<br/>firmware_f030/<br/>tp_f030_reference_inputs.c"]
    NFC["实＋参｜PN532 NFC<br/>执行读卡事务并提交卡片状态<br/>drivers/pn532.c<br/>drivers/pn532_i2c.c"]
    LOCK["实＋参｜门锁授权与输出<br/>根据授权结果驱动门锁并处理故障<br/>app/door_service.c<br/>firmware_f030/<br/>tp_f030_reference_lock.c"]
    S <---|"数据流：GPIO／EXTI<br/>资源快照和液滴事件"| INPUT
    S <-->|"I2C1：读卡命令／数据"| NFC
    S -->|"调用：授权状态机<br/>GPIO 锁输出"| LOCK
  PC <-->|"Modbus RTU／UART<br/>USART3＋RS485<br/>USART1＋RS232 可选"| M
  ESP <-->|"Modbus RTU／UART<br/>RS485 → USART3"| M
  ESP -.->|"可选 UART：USART6<br/>自定义暂存升级协议"| STAGE
  M <-->|"SPI2 命令／数据<br/>GPIO DRDY"| ADC
  M <---|"数据流：ADC1 采样<br/>温度队列快照"| TEMP
  M -->|"调用：GPIO 阀门<br/>TIM4 泵／TIM2 PWM"| OUT
  M <-->|"I2C：配置／日志／时间"| MEM
  M <-->|"UART 自定义状态协议<br/>UART4 ↔ USART1"| S
  classDef main fill:#dbeafe,stroke:#2563eb,color:#172554,stroke-width:3px;
  classDef cloud fill:#f3e8ff,stroke:#a855f7,color:#3b0764;
  classDef device fill:#ecfdf5,stroke:#059669,color:#064e3b;
  classDef slave fill:#fff7ed,stroke:#ea580c,color:#7c2d12;
  classDef optional fill:#f1f5f9,stroke:#64748b,color:#334155,stroke-dasharray:5 3;
  class M main;
  class PC,ESP,CLOUD cloud;
  class ADC,TEMP,OUT,MEM device;
  class S,INPUT,NFC,LOCK slave;
  class STAGE optional;
  linkStyle 5,8,9,10,12 stroke:#2563eb,stroke-width:2px;
```

F407 是 Modbus 从站；PC 或 ESP32 是主站。同一条 RS485 总线上只能有一个主站。若需同时连接，可选启用 USART1 供 PC 使用、USART3 供 ESP32 使用，并协调共享参数访问。

F407/F030 的 CRC 算法虽采用 CRC-16/Modbus，报文仍是项目自定义协议。从控状态未知、过期或带故障时接入启动自检及运行互锁。可选 GPIO 唤醒线和综合故障状态线作为辅助，液滴和故障细节仍经 UART 传输。PN532 是 F030 侧的可选装配；UID 白名单仅作教学，不构成防复制认证。云示例实现状态上报及可选 OTA 通知，未实现通用云端下发控制。

## 2. 主控分层及工程边界（完整图）

本图将七类职责展开为 14 个模块，保留业务调度、协议、检测内核、驱动、后台存储、升级以及并列工程之间的关系。图中的箭头表示真实调用或数据传递，不代表七层必须按目录顺序执行。蓝色线从业务装配延伸到控制调度、服务、状态机及采集执行驱动。

[打开完整 SVG（可放大）](assets/system-layers.svg) · [打开 PNG](assets/system-layers.png)

```mermaid
%%{init: {"theme":"base","themeVariables":{"fontFamily":"Microsoft YaHei, Arial, sans-serif","fontSize":"16px","primaryColor":"#f8fafc","primaryTextColor":"#172033","primaryBorderColor":"#94a3b8","lineColor":"#64748b","clusterBkg":"#f8fafc","clusterBorder":"#cbd5e1","edgeLabelBackground":"#ffffff"},"flowchart":{"htmlLabels":true,"curve":"linear","nodeSpacing":32,"rankSpacing":55,"padding":14,"wrappingWidth":320}}}%%
flowchart TB
    FW["参｜装配层 · F407 业务<br/>初始化外设、绑定 HAL 并创建任务<br/>firmware/main_business.c<br/>firmware/tp_f407_app.c"]
    FB["参｜装配层 · 独立启动器<br/>认证选槽并交接处理器<br/>firmware_boot/<br/>firmware/tp_f407_handoff.c"]
    OS["实｜RTOS 层 · 控制与通信<br/>队列串行化命令、优先停机并周期推进<br/>os/freertos_runtime.c<br/>os/freertos_subordinate.c"]
    BG["实＋参｜RTOS 层 · 后台与监督<br/>维护时间／日志快照并监督控制心跳<br/>os/freertos_clock.c<br/>os/freertos_event_log.c<br/>os/freertos_health.c"]
    PR["实｜协议层 · RTU 与主从协议<br/>分帧、校验并解析寄存器和从控状态<br/>protocol/modbus_rtu.c<br/>protocol/rtu_receiver.c<br/>protocol/subordinate_link.c"]
    SERVICE["实｜业务层 · 服务／计划／自检<br/>接受命令、选择模式并检查启动条件<br/>app/service.c、app/schedule.c<br/>app/preflight.c<br/>include/tp_service.h"]
    CTRL["实｜业务层 · 检测状态机<br/>推进配方、等待反馈并执行安全停止<br/>app/controller.c<br/>include/tp_core.h"]
    CALC["实｜业务层 · 测量／温控／标定<br/>计算 PID、浓度及自动标定／核查<br/>app/measurement.c<br/>app/auto_calibration.c"]
    ST["实｜存储层 · 参数与事件<br/>维护双槽配置及带校验的循环日志<br/>storage/config_store.c<br/>storage/event_log.c"]
    UP["实｜升级层 · 升级与安全启动<br/>验签、断点下载、试运行确认与回滚<br/>bootloader/signed_manifest.c<br/>bootloader/staged_upgrade.c<br/>bootloader/boot.c、bootloader/trial_confirm.c"]
    DR["实＋参｜驱动层 · 采集与执行<br/>封装 ADC、温度、泵、阀与加热操作<br/>drivers/ads1220.c、drivers/ntc.c<br/>firmware/tp_f407_pump.c<br/>firmware/tp_f407_valves.c<br/>firmware/tp_f407_heater.c"]
    IO["实＋参｜驱动层 · 存储与时钟<br/>访问 I2C 存储、RTC 和升级 Flash<br/>drivers/mb85rc16.c、drivers/ds3231.c<br/>drivers/w25q128.c<br/>firmware/tp_f407_image_flash.c"]
    OLD["旧工程｜Zephyr 任务与 BSP<br/>保留原有流程任务及板级外设实现<br/>项目根 src/main.c<br/>项目根 tasks/、bsp/"]
    Z["例｜Zephyr／POSIX 采集切片<br/>复用采集核心演示 ADC 到 Modbus<br/>zephyr_slice/<br/>examples/posix_slice/<br/>examples/posix_control/"]
  FW -->|"调用：创建控制<br/>与通信任务"| OS
  FW -->|"调用：创建后台<br/>与监督任务"| BG
  FW -->|"调用：初始化并绑定 HAL"| SERVICE
  OS -->|"调用：完整帧处理"| PR
  PR -->|"调用：寄存器服务回调"| SERVICE
  OS -->|"调用：周期 service_tick"| SERVICE
  SERVICE -->|"调用：start／tick／stop"| CTRL
  SERVICE -->|"调用：标定／核查服务"| CALC
  CTRL -->|"调用：PID／浓度计算"| CALC
  CTRL -->|"调用：HAL 经装配层转发"| DR
  OS <---|"数据流：配置保存请求"| SERVICE
  OS -->|"调用：后台配置保存"| ST
  BG -->|"调用：日志写入与查询"| ST
  BG <-->|"调用／数据流：RTC 快照"| IO
  ST -->|"调用：存储 I/O 回调"| IO
  FW -.->|"可选调用：升级服务<br/>与试运行确认任务"| UP
  FB -->|"调用：认证／选槽／回滚"| UP
  UP -->|"调用：镜像和元数据 I/O"| IO
  Z -.->|"复用调用：ADS1220／adc_reader"| DR
  classDef main fill:#dbeafe,stroke:#2563eb,color:#172554,stroke-width:2px;
  classDef device fill:#ecfdf5,stroke:#059669,color:#064e3b;
  classDef optional fill:#f3e8ff,stroke:#a855f7,color:#3b0764;
  classDef legacy fill:#f1f5f9,stroke:#94a3b8,color:#475569,stroke-dasharray:5 3;
  class FW,OS,SERVICE,CTRL,CALC main;
  class DR,IO,ST device;
  class UP,FB optional;
  class OLD,Z legacy;
  linkStyle 0,3,4,5,6,8,9 stroke:#2563eb,stroke-width:2px;
```


| 层／工程边界 | 一句话职责 | 对应目录与入口 |
|---|---|---|
| 业务内核 | 决定模式、自检、配方推进、温控、测量与故障处理 | `app/`＋`include/` |
| RTOS 适配 | 用任务和队列调度业务，并处理后台 I/O 与心跳监督 | `os/` |
| 协议 | 完成 Modbus RTU 分帧、寄存器服务及自定义主从状态解析 | `protocol/` |
| 驱动与板级适配 | 将器件事务与业务 HAL 回调连接到真实外设 | `drivers/`＋`firmware/tp_f407_*.c`；项目根 `bsp/` 属于旧 Zephyr 工程 |
| 固件装配 | 绑定依赖、创建任务，或构建独立启动器 | `firmware/`、`firmware_boot/`；项目根 `tasks/` 属于旧 Zephyr 工程 |
| 存储 | 持久保存配置与事件日志并处理校验／提交 | `storage/` |
| 升级 | 认证镜像、暂存安装、管理试运行确认与回滚 | `bootloader/` |

这不是逐层穿透的固定流水线：装配层负责依赖注入，RTOS 层负责线程和队列，业务通过 HAL 回调访问外设。`app/` 与 `include/` 不直接依赖 FreeRTOS 或 STM32。

必须区分三条代码线：FreeRTOS 主体位于 `freertos/`；项目根 `tasks/`、`bsp/` 使用 Zephyr API；`freertos/zephyr_slice/` 是共享采集核心的局部迁移示例。旧 `tasks/` 不能画成 FreeRTOS 的任务层，旧 `bsp/` 也不是当前 FreeRTOS 驱动的必经路径。`firmware_boot/` 是独立启动器装配，`bootloader/` 是可复用升级核心。

CMake/Ninja 属于构建工具，不参与运行时数据流。`freertos/CMakeLists.txt` 用于主机业务库与检查；`firmware/`、`firmware_f030/`、`firmware_boot/` 分别交叉构建对应固件。

## 3. 动态行为、存储布局和重点路径（第 2、3、4 步）

完整内容移至 [SYSTEM_RUNTIME.md](SYSTEM_RUNTIME.md)，取代本文件原先简略的启动、检测和故障流程，避免两份图示重复。

| 顺序 | 图 | 学习目标 |
|---|---|---|
| 第 2 步 | 2A 检测状态机 | 区分 IDLE、RUNNING、COMPLETE、FAILED 和受控等待 |
| 第 2 步 | 2B 完整检测配方 | 顺着 Modbus／计划、自检、取样、读数、反应、计算、清洗排液复述检测 |
| 第 2 步 | 2C 签名暂存 OTA | 区分暂存校验、复制安装、候选提交与重启 |
| 第 2 步 | 2D 启动元数据状态机 | 理解 READY、TRIAL、确认和回滚 |
| 第 2 步 | 2E 启动和任务创建 | 理解默认心跳、可选业务、初始化失败及自检条件 |
| 第 3 步 | 3A 存储布局 | 区分片内固件槽、外部暂存、业务 FRAM 和升级 FRAM |
| 第 3 步 | 3B 看门狗监督范围 | 区分控制心跳、数据过期和 OTA 健康确认 |
| 第 4 步 | 重点标注与证据表 | 蓝色／粗线突出 A 检测路径；列明待确认项并与 LEARNING 映射核对 |

## 4. 沿着完整框架复述设备行为

**启动：** 默认心跳固件不运行检测。选择业务配置后，`firmware/main_business.c` 进入业务装配，`firmware/tp_f407_app.c` 初始化外设、存储与业务对象，创建控制、通信和后台任务，再启动调度；教学配置由控制循环延后做一次上电自检，启动每次检测前仍重新检查。初始化失败必须安全停机；已创建部分任务也不能继续启动。可选独立启动器先认证并选择镜像，再交接到业务入口。具体分支见续篇图 2E。

**一次检测：** Modbus 命令／周期或每日计划 → 服务层启动检查 → 控制器依次取样、读空白与浊度、加试剂、加热与保温、停热、读反应信号、计算浓度、清洗和排液 → 关闭输出并进入 COMPLETE → 对外发布有效结果。ADS1220 提供光学采样，外部 NTC 提供温度，F030 状态参与自检和运行互锁；这些输入经 RTOS 缓存／队列进入控制逻辑。计算成功仍不等于检测完成，后续清洗或排液故障会使结果失效。具体状态和教学配方见续篇图 2A、2B。

**故障：** 启动检查失败时拒绝本次启动；运行故障或停止命令使控制器锁存故障、撤销结果并关热、停泵、关阀及取消采集。排除故障后，恢复流程依次执行安全输出、ADC 恢复、自检和清故障；成功回到 IDLE，不续跑旧配方。健康监督当前直接检查控制任务心跳，超时后尝试关闭输出并停止喂狗；温度／从控任务通过数据有效期间接影响安全判断，不能据此宣称所有任务均受心跳监督。详见续篇图 3B。

## 5. 与 LEARNING.md 的逐项核对

核对方式：先以自带文档为功能索引，再沿对应入口检查自有源码；核对“节点能否定位到文件”“箭头是否对应调用／数据／物理接口”“软件实现与实机验证是否分开”。没有以器件名称猜测接口，也没有将目录排列当作固定调用顺序。源码路径在写入前逐项检查存在性；Mermaid 两图均做离线渲染并人工检查。此次是文档布局修订，未重新运行固件测试，软件测试结论仍引用原项目文档。

| LEARNING 的“功能与代码位置”项 | 本文对应模块／续篇 | 核对入口 |
|---|---|---|
| 启动与任务 | 软件图的业务装配、RTOS；续篇 2E | `firmware/main_business.c`、`firmware/tp_f407_teaching_config.c`、`os/freertos_runtime.c` |
| 检测步骤、安全停止与自检 | 服务、状态机；续篇 2A／2B | `app/controller.c`、`app/service.c`、`app/preflight.c` |
| 测量、温控与标定 | 测量／温控／标定，ADS1220 与 NTC | `app/measurement.c`、`app/auto_calibration.c`、`drivers/` |
| 周期、每日定时和受控模式 | 业务服务／计划；续篇 2A／2B | `app/schedule.c`、`app/service.c` |
| 寄存器和上位机 | PC、协议层、ESP32 的 Modbus 连接 | `protocol/`、`docs/MODBUS.md`、`tools/tp_modbus.py`、`tools/tp_gui.py` |
| 参数和事件保存 | FRAM/RTC、后台任务、存储层；续篇 3A | `storage/config_store.c`、`storage/event_log.c` |
| 升级与从控 | 升级服务／核心／启动器、ESP32、F030 与门禁；续篇 2C／2D | `bootloader/`、`firmware_boot/`、`examples/esp32_cloud/`、`firmware_f030/` |
| 两种 RTOS 的采集示例 | 独立列出的 Zephyr／POSIX 切片 | `examples/posix_slice/`、`examples/posix_control/`、`zephyr_slice/` |

**待确认：** 实际板卡接线和外设配置、传感器与模拟前端、配方及标定数据、串口／SPI／I2C 时序、输出关断效果、看门狗复位、云端与 OTA 联调。本文不以参考软件或模拟结果证明这些条件已经满足。

## 6. Obsidian 中如何查看完整图

两张图均保留完整 Mermaid 源码，SVG／PNG 是同一份源码的渲染副本。完整架构包含长代码路径，放进窄正文后仍会缩小；查看总关系用整图，阅读节点细节可打开图上方的 SVG 或 PNG 链接并放大。

如需在正文中保持大字并横向滚动，可使用随文提供的 [可选 CSS 片段](assets/water-quality-architecture.css)：将其复制到**实际 Obsidian 仓库**的 `.obsidian/snippets/`，再在 Obsidian 中启用这个 CSS 片段。本文已经声明 `cssclasses: water-quality-architecture`，样式只针对带此标记的笔记；取消启用即可恢复。此处没有修改仓库的全局设置，也没有自动安装插件。

片段中的 `--architecture-diagram-width: 2200px` 控制图的最小阅读宽度；可按屏幕和字号偏好调整。它用于逐模块阅读，完整总览仍可通过 SVG 查看。若暂不启用 CSS，Mermaid 本身与图像链接都能使用。

验证边界：使用本机 Obsidian 1.14.4 安装包附带的 Mermaid，在临时无头浏览器中离线渲染并检查两张图；这不是对当前 Obsidian 窗口的实测，当前应用版本、主题或图形插件仍可能影响显示。没有修改固件源代码或 SYSTEM_RUNTIME.md。

布局修订：2026-11-03。项目路径相对引用，移动整个项目时请一起保留本文件、SYSTEM_RUNTIME.md 和 assets/。
