# Rename this file to config.py and supply your own local Wi-Fi credentials.
WIFI_SSID = "your-wifi-name"
WIFI_PASSWORD = "your-wifi-password"
PORT = 9000
UART_ID = 2
UART_TX = 17
UART_RX = 16
# Must exceed the STM32 upgrade frame timeout.
RECONNECT_DELAY_MS = 2000
# CA certificate for the HTTPS update server (PEM or DER as supported by MicroPython).
OTA_CA_FILE = "ca.pem"
