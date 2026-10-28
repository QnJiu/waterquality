"""Receive an Alibaba OTA notice and stage a signed bundle on F407."""
import json


class UpgradeNotice:
    def __init__(self, topic):
        self.topic = topic.encode()
        self.pending = None
        self.completed_version = None

    def callback(self, topic, payload):
        if topic != self.topic or self.pending is not None:
            return
        try:
            message = json.loads(payload)
            data = message["data"]
            url, version = data["url"], data["version"]
            if (not isinstance(url, str) or not url.startswith("https://") or
                    not isinstance(version, str) or not version or
                    version == self.completed_version):
                return
            self.pending = (url, version)
        except (ValueError, TypeError, KeyError):
            print("Invalid OTA notice")


def stage_bundle(url, config, uart_type, pin_type, clock):
    from ota_https import download
    from ota_client import Client, MAX_IMAGE_SIZE, upload_bundle

    with open("ota_bundle.tmp", "wb") as target:
        download(url, target, MAX_IMAGE_SIZE + 116, config.OTA_CA_FILE)
    uart = uart_type(config.OTA_UART_ID, baudrate=115200,
                     tx=pin_type(config.OTA_UART_TX), rx=pin_type(config.OTA_UART_RX),
                     timeout=0, timeout_char=0, rxbuf=1024, txbuf=1024)
    try:
        with open("ota_bundle.tmp", "rb") as source:
            return upload_bundle(Client(uart, clock), source)
    finally:
        uart.deinit()
