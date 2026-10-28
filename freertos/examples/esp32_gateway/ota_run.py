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


def run_https(manifest_url, image_url):
    """Fetch two HTTPS files, then stage them on F407."""
    import network
    import ntptime
    from ota_https import download
    from ota_client import MAX_IMAGE_SIZE

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(config.WIFI_SSID, config.WIFI_PASSWORD)
        deadline = time.ticks_add(time.ticks_ms(), 30000)
        while not wlan.isconnected() and time.ticks_diff(deadline, time.ticks_ms()) > 0:
            time.sleep_ms(200)
    if not wlan.isconnected():
        raise OSError("Wi-Fi connection timeout")
    ntptime.settime()
    with open("ota_manifest.tmp", "wb") as target:
        download(manifest_url, target, 116, config.OTA_CA_FILE)
    with open("ota_image.tmp", "wb") as target:
        download(image_url, target, MAX_IMAGE_SIZE, config.OTA_CA_FILE)
    with open("ota_manifest.tmp", "rb") as source:
        manifest = source.read(117)
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), timeout=0, timeout_char=0,
                rxbuf=1024, txbuf=1024)
    with open("ota_image.tmp", "rb") as image:
        result = upload(Client(uart, time), manifest, image)
    print("Staged bank %d, version %d, %d bytes" %
          (result["bank"], result["version"], result["written"]))


def run_bundle_https(bundle_url):
    """Download one signed-manifest-plus-image package and stage it."""
    import network
    import ntptime
    from ota_https import download
    from ota_client import MAX_IMAGE_SIZE, upload_bundle

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(config.WIFI_SSID, config.WIFI_PASSWORD)
        deadline = time.ticks_add(time.ticks_ms(), 30000)
        while not wlan.isconnected() and time.ticks_diff(deadline, time.ticks_ms()) > 0:
            time.sleep_ms(200)
    if not wlan.isconnected():
        raise OSError("Wi-Fi connection timeout")
    ntptime.settime()
    with open("ota_bundle.tmp", "wb") as target:
        download(bundle_url, target, MAX_IMAGE_SIZE + 116, config.OTA_CA_FILE)
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), timeout=0, timeout_char=0,
                rxbuf=1024, txbuf=1024)
    with open("ota_bundle.tmp", "rb") as source:
        result = upload_bundle(Client(uart, time), source)
    print("Staged bank %d, version %d, %d bytes" %
          (result["bank"], result["version"], result["written"]))
    return result
