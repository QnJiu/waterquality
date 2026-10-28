# ESP32 读取状态并上报阿里云（教学示例）

ESP32 使用 MicroPython 经 RS485 读取 F407 Modbus 地址 10–19，然后通过 TLS MQTT 发布阿里云物模型属性。通信出错或状态标志不符合寄存器定义时等待 5 秒并重连；失败时不发送旧数据。寄存器含义见 `docs/MODBUS.md`。

1. 给 ESP32 安装支持 `ssl.SSLContext`、`UART.flush()` 的 MicroPython 版本及新版 `umqtt.simple`，复制 `main.py`、`modbus_status.py`、`cloud.py`、`config.example.py` 到板上，将最后一个文件改名 `config.py`。
2. ESP32 TX/RX 经 3.3 V RS485 收发器连接 F407 USART3 RS485；DE 与 /RE 同接 `RS485_DE`，高电平发送，低电平接收。按收发器接线连接 A、B 和信号地，总线上只能有一个 Modbus 主站。F407 需启用并配置业务入口及 USART3，通信参数 115200 8N1。
3. 在 `config.py` 填写 Wi-Fi、F407 从站地址，以及阿里云设备的 TLS MQTT 主机名、client ID、用户名、密码和属性上报 Topic。连接参数须按所用实例、设备及其认证方式生成；示例不会替用户生成设备密钥。不要把实际凭据提交到 Git。
4. 将能验证所用服务端证书的 CA 证书放到 ESP32，并在 `CA_FILE` 配置文件名。程序用 NTP 设置时间，然后启用证书验证并使用端口 8883；网络需允许 NTP 和 MQTT TLS 连接。CA、时间或设备凭据错误时不会跳过认证。
5. 在阿里云物模型中创建属性标识符 `State`、`Fault`、`Step`、`StartResult`、`ResultValid`、`SaveStatus`、`WaitingNext`、`PreflightFaults`（整数，取值范围须覆盖 0–65535）、`TemperatureValid`（整数）以及 `Concentration`、`Temperature`（浮点数）。示例向 `/sys/<ProductKey>/<DeviceName>/thing/event/property/post` 发布 `thing.event.property.post`。三个新增属性分别表示参数保存状态、是否等待执行下一步和最近一次自检故障位，具体取值见寄存器 17–19。已有物模型也需添加这些属性。浓度结果无效时 `ResultValid=0` 且不发送 `Concentration`，云端可能仍保留上一次浓度，显示时必须检查有效标记。 温度通过单独的 Modbus 请求读取；无效时 `TemperatureValid=0` 且不发送 `Temperature`，云端显示时也必须检查有效标记。温度与其他状态是先后读取的快照，运行状态可能在两次读取之间变化。设备需要支持温度寄存器 128–130。

示例使用 QoS 0，打印 `MQTT connected` 只表示 MQTT 连接成功，不保证云平台已接收或接受每条属性；请在平台查看设备状态和属性日志。它不实现云端下发控制。代码已通过电脑侧模拟测试，尚未在 ESP32、RS485 或阿里云实测；引脚、电气连接和 TLS 能力须按实际板卡确认。

## 可选的云端升级通知

将 `ota_notice.py` 以及 `examples/esp32_gateway` 中的 `ota_client.py`、`ota_https.py` 一并复制到 ESP32。将 `OTA_ENABLED` 设为 `True`，配置产品和设备对应的 `OTA_TOPIC`、`OTA_INFORM_TOPIC`、`OTA_PROGRESS_TOPIC`、下载服务器的 CA，以及第二路串口 `OTA_UART_ID/TX/RX`。第一路 UART 继续用于 RS485 Modbus；第二路 UART 以 3.3 V TTL 接 F407 USART6，TX/RX 交叉并共地。选择适用于具体 ESP32 的两个串口和引脚。

云端固件文件需使用 `tools/pack_ota_bundle.py` 生成 `.tpb`，通知中的 `data.url` 必须是该文件的 HTTPS 地址，`data.version` 必须为签名清单中固件版本的十进制字符串（例如 `12`，范围 1–4294967295，不带前导零）；创建云端固件时也使用相同的版本号。下载后会比对清单版本，不一致则拒绝暂存。下载限制与网关 HTTPS 示例相同，ESP32 文件系统需要容纳完整升级包。MQTT 在处理下载和串口传输之前断开，完成后重新连接；同一次运行中已暂存的版本不会重复执行。F407 负责验证签名及镜像。

打印 `Staged` 只表示暂存成功；复制、重启、试运行和确认仍由 F407 处理。ESP32 读取 F407 Modbus 119–121，仅在有效标记为 1 时将已确认版本上报到 `OTA_INFORM_TOPIC`，每次 MQTT 重连和版本变化后重新上报。版本必须与云端的版本字符串完全一致，平台是否收到及判定成功需在云端查看；QoS 0 不保证送达。暂存后重连时上报进度 1，明确的暂存失败上报 -1；如果最后的 FINISH 回复超时，结果未知，上报进度 1 并继续等待设备确认，只有读到目标固件的已确认版本才上报 100。若最终没有确认，需检查设备启动状态或重新发起升级。发送升级前，ESP32 将目标版本保存到 `OTA_STATE_FILE`（默认 `ota_pending.txt`）；重启后继续等待设备确认并上报进度 100。重启不会自动重新下载，若升级未完成可由云端重新下发相同版本的通知；重试失败不会丢弃此前待确认的记录。待确认期间拒绝不同版本的通知，避免覆盖旧状态。若已排查并确认旧升级无法完成，可停止程序、删除该记录文件并重启，再重新下发升级。其他进度与去重状态保存在内存中，QoS 0 不保证送达。文件系统需要可写；写入失败时不会开始暂存。更新记录时部分文件系统不支持覆盖重命名，断电可能丢失记录，损坏记录会被忽略；这不改变 F407 的升级和回滚状态。需要先完成 F407 启动器及存储装配；通知和下载仍需实际阿里云与硬件联调。
