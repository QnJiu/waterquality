# ESP32 Wi-Fi 升级网关（教学参考）

ESP32 运行 MicroPython，通过 Wi-Fi TCP 把升级数据转到 F407 的 USART6；升级帧、签名验证、镜像写入和回滚沿用项目中已有的暂存升级服务。网关只转发字节，不解析固件，也不需要修改原有升级协议。

1. 在 ESP32 上安装与硬件匹配的 MicroPython，把 `main.py` 和 `config.example.py` 复制过去，后者改名为 `config.py`，填写 Wi-Fi 和端口。默认使用 UART2、GPIO17 TX、GPIO16 RX，115200 8N1；引脚可修改，须先确认 ESP32 板卡可用。
2. ESP32 TX 接 F407 PC7（USART6 RX），ESP32 RX 接 F407 PC6（USART6 TX），共地，确保两侧电平兼容。F407 需要配置并启动 `tp_f407_stage_reference_start` 和升级存储，详见 `firmware/README.md`。`RECONNECT_DELAY_MS` 必须大于 F407 暂存服务配置的帧超时。
3. 启动网关，终端显示 IP 后，在电脑运行：

   `python tools/upgrade_client.py --tcp 192.168.1.100:9000 --manifest signed.tpu --image app.bin`

   地址和文件换成自己的。电脑端也支持 HTTPS 下载清单与镜像；参考 `python tools/upgrade_client.py --help`。客户端逐帧等待回复，不要用其他程序持续向网关高速发送数据。

一次只服务一个 TCP 客户端；发送不完整时会继续发送，断线后等待帧超时再接受下一个客户端。网关会尝试 Wi-Fi 重连。串口没有硬件流控：网络拥堵、ESP32 重启或串口溢出可能导致传输失败，上位机应查询状态并重新连接/续传。固件签名校验不能代替网络访问控制：TCP 未加密且没有客户端认证，只应放在可信隔离网络中。连接实际阿里云需要另外实现云端设备协议和安全连接；本例不是阿里云客户端。以上仅通过电脑侧模拟测试，ESP32、网络和接线尚未实测。
