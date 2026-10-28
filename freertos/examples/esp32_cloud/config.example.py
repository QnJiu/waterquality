# Select pins that are valid for the specific ESP32 board.
UART_ID = 2
UART_TX = 17
UART_RX = 16
RS485_DE = 4
MODBUS_ADDRESS = 1

# Keep this file private. Fill in TLS MQTT connection parameters for your device.
WIFI_SSID = "your-wifi-name"
WIFI_PASSWORD = "your-wifi-password"
MQTT_HOST = "your-instance-hostname"
MQTT_CLIENT_ID = "your-device-client-id"
MQTT_USER = "your-device-username"
MQTT_PASSWORD = "your-device-password"
MQTT_TOPIC = "/sys/your-product-key/your-device-name/thing/event/property/post"
CA_FILE = "ca.pem"
PUBLISH_INTERVAL_S = 10

# Optional OTA: use a second UART for F407 USART6, separate from Modbus.
OTA_ENABLED = False
OTA_TOPIC = "/ota/device/upgrade/your-product-key/your-device-name"
OTA_PROGRESS_TOPIC = "/ota/device/progress/your-product-key/your-device-name"
OTA_INFORM_TOPIC = "/ota/device/inform/your-product-key/your-device-name"
OTA_UART_ID = 1
OTA_UART_TX = 25
OTA_UART_RX = 26
OTA_CA_FILE = "ota-ca.pem"

# Retain the target version across ESP32 restarts.
OTA_STATE_FILE = "ota_pending.txt"
