# ESP32 读取状态并上报阿里云（教学示例）

ESP32 使用 MicroPython 经 RS485 读取 F407 Modbus 地址 10–16，然后通过 TLS MQTT 发布阿里云物模型属性。通信出错时等待 5 秒并重连；失败时不发送旧数据。寄存器含义见 `docs/MODBUS.md`。

1. 给 ESP32 安装支持 `ssl.SSLContext`、`UART.flush()` 的 MicroPython 版本及新版 `umqtt.simple`，复制 `main.py`、`modbus_status.py`、`cloud.py`、`config.example.py` 到板上，将最后一个文件改名 `config.py`。
2. ESP32 TX/RX 经 3.3 V RS485 收发器连接 F407 USART3 RS485；DE 与 /RE 同接 `RS485_DE`，高电平发送，低电平接收。按收发器接线连接 A、B 和信号地，总线上只能有一个 Modbus 主站。F407 需启用并配置业务入口及 USART3，通信参数 115200 8N1。
3. 在 `config.py` 填写 Wi-Fi、F407 从站地址，以及阿里云设备的 TLS MQTT 主机名、client ID、用户名、密码和属性上报 Topic。连接参数须按所用实例、设备及其认证方式生成；示例不会替用户生成设备密钥。不要把实际凭据提交到 Git。
4. 将能验证所用服务端证书的 CA 证书放到 ESP32，并在 `CA_FILE` 配置文件名。程序用 NTP 设置时间，然后启用证书验证并使用端口 8883；网络需允许 NTP 和 MQTT TLS 连接。CA、时间或设备凭据错误时不会跳过认证。
5. 在阿里云物模型中创建属性标识符 `State`、`Fault`、`Step`、`StartResult`、`ResultValid`（整数）和 `Concentration`（浮点数）。示例向 `/sys/<ProductKey>/<DeviceName>/thing/event/property/post` 发布 `thing.event.property.post`。浓度结果无效时 `ResultValid=0` 且不发送 `Concentration`，云端可能仍保留上一次浓度，显示时必须检查有效标记。

示例使用 QoS 0，打印 `MQTT connected` 只表示 MQTT 连接成功，不保证云平台已接收或接受每条属性；请在平台查看设备状态和属性日志。它不实现云端下发控制或云端 OTA。代码已通过电脑侧模拟测试，尚未在 ESP32、RS485 或阿里云实测；引脚、电气连接和 TLS 能力须按实际板卡确认。
