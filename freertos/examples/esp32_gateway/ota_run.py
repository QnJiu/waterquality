"""Run from the REPL after copying signed.tpu and app.bin to ESP32 storage."""
import time
from machine import UART, Pin
import config
from ota_client import Client, upload


def run():
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), timeout=0, timeout_char=0,
                rxbuf=1024, txbuf=1024)
    with open("signed.tpu", "rb") as source:
        manifest = source.read(117)
    with open("app.bin", "rb") as image:
        result = upload(Client(uart, time), manifest, image)
    print("Staged bank %d, version %d, %d bytes" %
          (result["bank"], result["version"], result["written"]))
