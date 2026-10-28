"""MicroPython ESP32 Wi-Fi to UART bridge for the TP upgrade protocol.

Copy this file and config.py to the board, then run main(). One TCP client is
served at a time. The firmware on the STM32 validates the signed image.
"""

BUFFER_SIZE = 512


def _would_block(exc):
    return bool(exc.args) and exc.args[0] in (11, 35, 10035)


class Bridge:
    """Move bounded chunks in both directions, including partial writes."""

    def __init__(self, sock, uart):
        self.sock = sock
        self.uart = uart
        self.to_uart = b""
        self.to_socket = b""

    def step(self):
        if self.to_uart:
            n = self.uart.write(self.to_uart) or 0
            if n < 0 or n > len(self.to_uart):
                raise OSError("invalid UART write count")
            self.to_uart = self.to_uart[n:]
        if self.to_socket:
            try:
                n = self.sock.send(self.to_socket)
            except OSError as exc:
                if not _would_block(exc):
                    raise
                n = None
            if n == 0:
                return False
            if n is None:
                n = 0
            if n < 0 or n > len(self.to_socket):
                raise OSError("invalid socket write count")
            self.to_socket = self.to_socket[n:]
        if not self.to_uart:
            try:
                data = self.sock.recv(BUFFER_SIZE)
            except OSError as exc:
                if not _would_block(exc):
                    raise
                data = None
            if data == b"":
                return False
            if data:
                self.to_uart = data
        if not self.to_socket and self.uart.any():
            self.to_socket = self.uart.read(BUFFER_SIZE) or b""
        return True


def main():
    import network
    import socket
    import time
    from machine import UART, Pin
    import config

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    uart = UART(config.UART_ID, baudrate=115200, tx=Pin(config.UART_TX),
                rx=Pin(config.UART_RX), rxbuf=1024, txbuf=512, timeout=0, timeout_char=0)
    while True:
        if not wlan.isconnected():
            try:
                wlan.connect(config.WIFI_SSID, config.WIFI_PASSWORD)
            except OSError:
                time.sleep_ms(5000)
                continue
            deadline = time.ticks_add(time.ticks_ms(), 30000)
            while not wlan.isconnected() and time.ticks_diff(deadline, time.ticks_ms()) > 0:
                time.sleep_ms(100)
            if not wlan.isconnected():
                time.sleep_ms(5000)
                continue
        server = socket.socket()
        try:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(("0.0.0.0", config.PORT))
            server.listen(1)
            server.settimeout(1)
            print("TP gateway listening:", wlan.ifconfig()[0], config.PORT)
            while wlan.isconnected():
                try:
                    client, _ = server.accept()
                except OSError as exc:
                    if _would_block(exc) or (exc.args and exc.args[0] == 110):
                        continue
                    raise
                try:
                    client.setblocking(False)
                    # Discard stale replies from a previous connection.
                    if uart.any():
                        uart.read(1024)
                    bridge = Bridge(client, uart)
                    while wlan.isconnected() and bridge.step():
                        time.sleep_ms(1)
                except OSError:
                    pass
                finally:
                    client.close()
                # Let an incomplete STM32 frame time out before another client.
                time.sleep_ms(config.RECONNECT_DELAY_MS)
        except OSError as exc:
            print("Gateway network error:", exc)
            time.sleep_ms(1000)
        finally:
            server.close()


if __name__ == "__main__":
    main()
