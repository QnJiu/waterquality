# ESP32 Wi-Fi 升级网关（教学参考）

ESP32 运行 MicroPython，通过 Wi-Fi TCP 把升级数据转到 F407 的 USART6；升级帧、签名验证、镜像写入和回滚沿用项目中已有的暂存升级服务。网关只转发字节，不解析固件，也不需要修改原有升级协议。

1. 在 ESP32 上安装与硬件匹配的 MicroPython，把 `main.py` 和 `config.example.py` 复制过去，后者改名为 `config.py`，填写 Wi-Fi 和端口。默认使用 UART2、GPIO17 TX、GPIO16 RX，115200 8N1；引脚可修改，须先确认 ESP32 板卡可用。
2. ESP32 TX 接 F407 PC7（USART6 RX），ESP32 RX 接 F407 PC6（USART6 TX），共地，确保两侧电平兼容。F407 需要配置并启动 `tp_f407_stage_reference_start` 和升级存储，详见 `firmware/README.md`。`RECONNECT_DELAY_MS` 必须大于 F407 暂存服务配置的帧超时。
3. 启动网关，终端显示 IP 后，在电脑运行：

   `python tools/upgrade_client.py --tcp 192.168.1.100:9000 --manifest signed.tpu --image app.bin`

   地址和文件换成自己的。电脑端也支持 HTTPS 下载清单与镜像；参考 `python tools/upgrade_client.py --help`。客户端逐帧等待回复，不要用其他程序持续向网关高速发送数据。

一次只服务一个 TCP 客户端；发送不完整时会继续发送，断线后等待帧超时再接受下一个客户端。网关会尝试 Wi-Fi 重连。串口没有硬件流控：网络拥堵、ESP32 重启或串口溢出可能导致传输失败，上位机应查询状态并重新连接/续传。固件签名校验不能代替网络访问控制：TCP 未加密且没有客户端认证，只应放在可信隔离网络中。连接实际阿里云需要另外实现云端设备协议和安全连接；本例不是阿里云客户端。以上仅通过电脑侧模拟测试，ESP32、网络和接线尚未实测。

## ESP32 本地文件升级

也可以不运行 TCP 网关，直接让 ESP32 发送存储中的升级文件。停止正在运行的 `main.py`，将 `ota_client.py`、`ota_run.py`、配置文件、签名清单 `signed.tpu` 和对应的 `app.bin` 拷贝到 ESP32，确认 ESP32 剩余存储足够容纳镜像。在 REPL 中执行：

```python
import ota_run
ota_run.run()
```

客户端先分块计算镜像 SHA-256，与清单核对后才向 F407 发起升级；F407 仍负责清单签名验证、镜像认证和试运行回滚。发送时按 256 字节读取，丢失回复后查询暂存进度并续传；运行期间请勿修改镜像，确保此时 UART 没有其他任务占用。重启后的再发送会检查清单身份和已有进度。`Staged` 表示暂存完成，后续复制、启动和试运行确认仍取决于 F407 的升级服务装配。

此方式使用已经放在 ESP32 上的文件；签名验证及 Flash 动作由 F407 执行。代码通过主机模拟测试，尚未在 ESP32 和 F407 上实测。

## 手动 HTTPS 下载并升级

复制 `ota_https.py`，在 `config.py` 设置 Wi-Fi、`OTA_CA_FILE`，并将能验证下载服务器证书的 CA 文件复制到 ESP32。固件需支持 `ssl.SSLContext` 和证书验证，网络需要能访问 NTP 和下载服务器。停止 TCP 网关后在 REPL 执行：

```python
import ota_run
ota_run.run_https("https://example.com/signed.tpu", "https://example.com/app.bin")
```

将示例地址改为实际地址；服务端必须直接返回 HTTP 200、`Content-Length` 和完整文件，暂不支持跳转、分块传输和自定义端口。下载会占用 ESP32 文件系统，文件写到 `ota_manifest.tmp` 和 `ota_image.tmp`，后一次运行会覆盖它们。镜像下载完成并核对清单后才开始发送到 F407；不匹配时不会启动升级。成功暂存后这两个文件可手动删除。下载支持的最大固件大小为 384 KiB。

阿里云 OTA 通知、下载地址的自动接收及升级状态上报仍未接入；此入口使用手动指定的地址。HTTPS 校验和串口流程目前只经过主机模拟，尚未进行实机或真实云服务测试。
