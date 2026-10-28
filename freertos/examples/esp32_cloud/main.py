"""Read Modbus status and publish Alibaba IoT properties over TLS MQTT."""
import time
import network
import ntptime
import ssl
from machine import UART, Pin
from umqtt.simple import MQTTClient, MQTTException
import config
from modbus_status import read_status
from cloud import publish_status


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
    while True:
        client = None
        try:
            wlan = connect_wifi()
            client = connect_mqtt()
            print("MQTT connected")
            while wlan.isconnected():
                status = read_status(uart, direction, config.MODBUS_ADDRESS, time)
                message_id += 1
                publish_status(client, config.MQTT_TOPIC, status, message_id)
                time.sleep_ms(config.PUBLISH_INTERVAL_S * 1000)
        except (OSError, ValueError, MQTTException) as exc:
            print("Cloud or Modbus error:", exc)
        finally:
            if client and client.sock:
                client.sock.close()
        time.sleep_ms(5000)


if __name__ == "__main__":
    main()
