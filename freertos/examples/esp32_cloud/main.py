"""Read Modbus status and publish Alibaba IoT properties over TLS MQTT."""
import time
import network
import ntptime
import ssl
from machine import UART, Pin
from umqtt.simple import MQTTClient, MQTTException
import config
from modbus_status import read_status, read_confirmed_version
from cloud import publish_status, publish_version
from ota_notice import UpgradeNotice


def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(config.WIFI_SSID, config.WIFI_PASSWORD)
        deadline = time.ticks_add(time.ticks_ms(), 30000)
        while not wlan.isconnected() and time.ticks_diff(deadline, time.ticks_ms()) > 0:
            time.sleep_ms(200)
    if not wlan.isconnected():
        raise OSError("Wi-Fi connection timeout")
    return wlan


def connect_mqtt():
    # Certificate checks require the correct current date and a trusted CA.
    ntptime.settime()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.verify_mode = ssl.CERT_REQUIRED
    context.load_verify_locations(cafile=config.CA_FILE)
    client = MQTTClient(config.MQTT_CLIENT_ID.encode(), config.MQTT_HOST,
                        port=8883, user=config.MQTT_USER.encode(),
                        password=config.MQTT_PASSWORD.encode(), keepalive=60,
                        ssl=context)
    try:
        client.connect(timeout=10)
    except (OSError, MQTTException):
        if client.sock:
            client.sock.close()
        raise
    return client


def main():
    if not 1 <= config.PUBLISH_INTERVAL_S <= 30:
        raise ValueError("PUBLISH_INTERVAL_S must be between 1 and 30")
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), timeout=0)
    direction = Pin(config.RS485_DE, Pin.OUT, value=0)
    message_id = 0
    notice = UpgradeNotice(config.OTA_TOPIC) if config.OTA_ENABLED else None
    while True:
        client = None
        try:
            wlan = connect_wifi()
            client = connect_mqtt()
            print("MQTT connected")
            if notice:
                client.set_callback(notice.callback)
                client.subscribe(config.OTA_TOPIC.encode(), qos=0)
            last_version = None
            next_publish = time.ticks_ms()
            while wlan.isconnected():
                if notice:
                    client.check_msg()
                    if notice.pending:
                        break
                if time.ticks_diff(time.ticks_ms(), next_publish) >= 0:
                    status = read_status(uart, direction, config.MODBUS_ADDRESS, time)
                    message_id += 1
                    publish_status(client, config.MQTT_TOPIC, status, message_id)
                    if notice:
                        version = read_confirmed_version(uart, direction, config.MODBUS_ADDRESS, time)
                        if version is None:
                            last_version = None
                        elif version != last_version:
                            message_id += 1
                            publish_version(client, config.OTA_INFORM_TOPIC, version, message_id)
                            last_version = version
                    next_publish = time.ticks_add(time.ticks_ms(), config.PUBLISH_INTERVAL_S * 1000)
                time.sleep_ms(1000)
        except (OSError, ValueError, MQTTException) as exc:
            print("Cloud or Modbus error:", exc)
        finally:
            if client and client.sock:
                client.sock.close()
        if notice and notice.pending:
            from ota_notice import stage_bundle
            from ota_client import ProtocolError
            url, version = notice.pending
            notice.pending = None
            try:
                result = stage_bundle(url, version, config, UART, Pin, time)
                notice.completed_version = version
                print("Staged bank %d, firmware version %d; restart and confirmation pending" %
                      (result["bank"], result["version"]))
            except (OSError, ValueError, ProtocolError) as exc:
                print("OTA staging failed:", exc)
        time.sleep_ms(5000)


if __name__ == "__main__":
    main()
