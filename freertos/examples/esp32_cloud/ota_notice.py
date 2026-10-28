"""Receive an Alibaba OTA notice and stage a signed bundle on F407."""
import json
import os
import struct


def valid_version(version):
    return (isinstance(version, str) and version.isdigit() and len(version) <= 10
            and not version.startswith("0") and int(version) <= 0xffffffff)


def load_pending(path):
    try:
        with open(path, "r") as source:
            version = source.read(11)
    except OSError:
        return None
    if not valid_version(version):
        print("Invalid saved OTA version; wait for a new notice")
        return None
    return version


def save_pending(path, version):
    if not valid_version(version):
        raise ValueError("invalid OTA version")
    previous = load_pending(path)
    if previous == version:
        return
    if previous is not None:
        raise ValueError("another OTA version is awaiting confirmation")
    # Write before touching the old record, then replace it.  On filesystems
    # without overwrite-rename, removal leaves a small interruption window.
    temp = path + ".tmp"
    with open(temp, "w") as target:
        target.write(version)
        target.flush()
    try:
        os.rename(temp, path)
    except OSError:
        try:
            os.remove(path)
        except OSError:
            pass
        os.rename(temp, path)


def clear_pending(path):
    try:
        os.remove(path)
    except OSError:
        # Missing is harmless; leave other errors visible to the caller.
        try:
            os.stat(path)
        except OSError:
            return
        raise


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
                    not valid_version(version) or version == self.completed_version):
                return
            self.pending = (url, version)
        except (ValueError, TypeError, KeyError):
            print("Invalid OTA notice")


def stage_bundle(url, version, config, uart_type, pin_type, clock):
    from ota_https import download
    from ota_client import Client, MAX_IMAGE_SIZE, upload_bundle

    with open("ota_bundle.tmp", "wb") as target:
        download(url, target, MAX_IMAGE_SIZE + 116, config.OTA_CA_FILE)
    # The cloud version is the canonical decimal representation of the signed
    # manifest version; never start staging a differently numbered package.
    with open("ota_bundle.tmp", "rb") as source:
        header = source.read(12)
    if len(header) != 12 or header[:4] != b"TPU1" or struct.unpack("<I", header[8:12])[0] != int(version):
        raise ValueError("OTA version does not match manifest")
    uart = uart_type(config.OTA_UART_ID, baudrate=115200,
                     tx=pin_type(config.OTA_UART_TX), rx=pin_type(config.OTA_UART_RX),
                     timeout=0, timeout_char=0, rxbuf=1024, txbuf=1024)
    try:
        with open("ota_bundle.tmp", "rb") as source:
            return upload_bundle(Client(uart, clock), source)
    finally:
        uart.deinit()
