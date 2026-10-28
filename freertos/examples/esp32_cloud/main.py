"""Print controller status once per second; copy config.example.py to config.py."""
import time
import json
from machine import UART, Pin
import config
from modbus_status import read_status


def main():
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), timeout=0)
    direction = Pin(config.RS485_DE, Pin.OUT, value=0)
    while True:
        try:
            print(json.dumps(read_status(uart, direction, config.MODBUS_ADDRESS, time)))
        except (OSError, ValueError) as exc:
            print("Modbus status unavailable:", exc)
        time.sleep_ms(1000)


if __name__ == "__main__":
    main()
