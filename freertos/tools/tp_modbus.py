"""Small Modbus RTU client for the TP teaching firmware (requires pyserial)."""
import argparse
import json
import math
import struct
import sys
import time


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


def frame(payload):
    return payload + struct.pack('<H', crc16(payload))


FLOAT_FIELDS = ('slope', 'intercept', 'turbidity_factor', 'min_signal',
                'max_concentration', 'kp', 'ki', 'kd', 'deadband_c',
                'output_max', 'integral_max', 'max_temperature_c')


class ModbusError(Exception):
    pass


class Client:
    def __init__(self, port, address, timeout=2.0):
        if not 1 <= address <= 247 or timeout <= 0:
            raise ValueError('invalid slave address or timeout')
        self.port, self.address, self.timeout = port, address, timeout

    def exchange(self, request, expected):
        self.port.reset_input_buffer()
        packet = frame(bytes([self.address]) + request)
        if self.port.write(packet) != len(packet):
            raise ModbusError('short serial write')
        self.port.flush()
        deadline = time.monotonic() + self.timeout
        reply = bytearray()
        length = None
        while time.monotonic() < deadline:
            if len(reply) >= 2 and reply[1] & 0x80:
                length = 5
            elif len(reply) >= 3:
                length = 5 + reply[2] if reply[1] == 3 else 8
            if length is not None and len(reply) >= length:
                break
            # Set short per-read timeouts so the overall deadline is bounded.
            self.port.timeout = min(0.1, max(0, deadline - time.monotonic()))
            part = self.port.read(1)
            if part:
                reply.extend(part)
        if length is None or len(reply) != length:
            raise ModbusError('response timeout or truncated frame')
        if crc16(reply[:-2]) != int.from_bytes(reply[-2:], 'little'):
            raise ModbusError('response CRC mismatch')
        if reply[0] != self.address or reply[1] not in (request[0], request[0] | 0x80):
            raise ModbusError('unexpected slave or function')
        if reply[1] & 0x80:
            raise ModbusError('device exception %d' % reply[2])
        if reply[1] == 3:
            if reply[2] != expected * 2:
                raise ModbusError('unexpected register count')
            return list(struct.unpack('>' + 'H' * expected, reply[3:-2]))
        if bytes(reply[2:6]) != request[1:5]:
            raise ModbusError('write acknowledgement mismatch')
        return None

    def read(self, address, count):
        if not 0 <= address <= 0xffff or not 1 <= count <= 125 or address + count > 65536:
            raise ValueError('invalid read range')
        return self.exchange(struct.pack('>BHH', 3, address, count), count)

    def command(self, command):
        if command not in range(1, 9):
            raise ValueError('invalid command')
        self.exchange(struct.pack('>BHH', 6, 0, command), 0)

    def schedule(self, mode, seconds, minute, offset):
        if mode not in range(3) or not 1 <= seconds <= 2147483 or not 0 <= minute < 1440 or not -720 <= offset <= 840:
            raise ValueError('invalid schedule')
        values = (mode, seconds >> 16, seconds & 65535, minute, offset & 65535)
        request = struct.pack('>BHHB5H', 16, 1, 5, 10, *values)
        self.exchange(request, 0)

    def get_config(self):
        words = self.read(20, 27)
        result = {name: float32(words[2*i], words[2*i+1])
                  for i, name in enumerate(FLOAT_FIELDS)}
        result['period_seconds'] = (words[24] << 16) | words[25]
        result['slave_address'] = words[26]
        return result

    def set_config(self, config):
        if not isinstance(config, dict) or set(config) != set(FLOAT_FIELDS) | {'period_seconds', 'slave_address'}:
            raise ValueError('config must contain exactly the documented fields')
        words = []
        for name in FLOAT_FIELDS:
            value = config[name]
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise ValueError('invalid numeric value for ' + name)
            try:
                packed = struct.pack('>f', value)
            except (OverflowError, struct.error):
                raise ValueError('float out of range for ' + name) from None
            if not math.isfinite(struct.unpack('>f', packed)[0]):
                raise ValueError('float out of range for ' + name)
            words.extend(struct.unpack('>HH', packed))
        seconds, address = config['period_seconds'], config['slave_address']
        if type(seconds) is not int or not 1 <= seconds <= 2147483 or type(address) is not int or not 1 <= address <= 247:
            raise ValueError('invalid period_seconds or slave_address')
        words.extend((seconds >> 16, seconds & 65535, address))
        request = struct.pack('>BHHB27H', 16, 20, 27, 54, *words)
        self.exchange(request, 0)

    def calibrate(self, values):
        if len(values) != 9 or not all(math.isfinite(x) for x in values):
            raise ValueError('calibration requires nine finite numbers')
        try:
            data = struct.pack('>9f', *values)
        except (OverflowError, struct.error):
            raise ValueError('calibration value is outside float32 range') from None
        self.exchange(struct.pack('>BHHB', 16, 70, 18, 36) + data, 0)

    def check_standard(self, values):
        if len(values) != 5 or not all(math.isfinite(x) for x in values):
            raise ValueError('standard check requires five finite numbers')
        try:
            data = struct.pack('>5f', *values)
        except (OverflowError, struct.error):
            raise ValueError('standard check value is outside float32 range') from None
        self.exchange(struct.pack('>BHHB', 16, 92, 10, 20) + data, 0)
        state, high, low = self.read(102, 3)
        if state not in (1, 2):
            raise ModbusError('standard check result unavailable')
        return {'passed': state == 1, 'actual_mg_l': float32(high, low)}

    def auto_calibration_status(self):
        state, error = self.read(90, 2)
        names = ('unavailable', 'idle', 'low', 'high', 'complete', 'failed', 'cancelled')
        return {'state': names[state] if state < len(names) else 'unknown',
                'state_code': state, 'error_code': error}

    def log(self, sequence=None):
        if sequence is not None and not 0 <= sequence <= 0xffffffff:
            raise ValueError('invalid event sequence')
        seq = sequence if sequence is not None else 0
        request = struct.pack('>BHHB3H', 16, 50, 3, 6,
                              1 if sequence is not None else 0, seq >> 16, seq & 65535)
        self.exchange(request, 0)
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            v = self.read(53, 12)
            if v[0] == 3:
                seconds = int.from_bytes(struct.pack('>4H', *v[6:10]), 'big', signed=True)
                return {'sequence': (v[1] << 16) | v[2], 'type': v[3], 'detail': v[4],
                        'utc': seconds if v[5] else None, 'value_bits': (v[10] << 16) | v[11]}
            if v[0] != 2:
                raise ModbusError('log query failed, status %d' % v[0])
            time.sleep(0.05)
        raise ModbusError('log query timeout')


def float32(high, low):
    return struct.unpack('>f', struct.pack('>HH', high, low))[0]


def main(argv=None):
    p = argparse.ArgumentParser(description='TP instrument Modbus RTU client')
    p.add_argument('--port', required=True, help='serial port, e.g. COM3')
    p.add_argument('--baud', type=int, default=115200)
    p.add_argument('--address', type=int, default=1)
    p.add_argument('--timeout', type=float, default=2.0)
    sub = p.add_subparsers(dest='action', required=True)
    sub.add_parser('status')
    sub.add_parser('auto-cal-status', help='show automatic calibration progress and error')
    calibrate = sub.add_parser('calibrate')
    calibrate.add_argument('values', type=float, nargs=9, metavar='VALUE',
                           help='low blank/reacted/turbidity/concentration, high blank/reacted/turbidity/concentration, minimum delta')
    standard = sub.add_parser('check-standard', help='evaluate a measured standard')
    standard.add_argument('values', type=float, nargs=5, metavar='VALUE',
                          help='blank reacted turbidity concentration tolerance (mg/L)')
    get_config = sub.add_parser('get-config')
    get_config.add_argument('--output', help='save JSON as UTF-8')
    config = sub.add_parser('set-config')
    config.add_argument('file', help='complete JSON configuration file')
    log = sub.add_parser('log')
    log.add_argument('--sequence', type=int, help='omit for latest event')
    cmd = sub.add_parser('command')
    cmd.add_argument('name', choices=['start', 'stop', 'clear', 'save', 'next', 'auto-cal-start', 'auto-cal-apply', 'adc-recover'])
    schedule = sub.add_parser('schedule')
    schedule.add_argument('mode', choices=['controlled', 'periodic', 'daily'])
    schedule.add_argument('seconds', type=int)
    schedule.add_argument('minute', type=int, help='local minute of day, 0..1439')
    schedule.add_argument('utc_offset', type=int, help='UTC offset in minutes')
    args = p.parse_args(argv)
    try:
        import serial
        with serial.Serial(args.port, args.baud, bytesize=8, parity='N', stopbits=1, timeout=0.1, write_timeout=args.timeout) as port:
            client = Client(port, args.address, args.timeout)
            if args.action == 'command':
                client.command(['start', 'stop', 'clear', 'save', 'next', 'auto-cal-start', 'auto-cal-apply', 'adc-recover'].index(args.name) + 1)
                print('Command accepted; check status for completion.')
            elif args.action == 'auto-cal-status':
                print(json.dumps(client.auto_calibration_status()))
            elif args.action == 'schedule':
                client.schedule(['controlled', 'periodic', 'daily'].index(args.mode), args.seconds, args.minute, args.utc_offset)
                print('Schedule updated in RAM; use command save and check status to persist.')
            elif args.action == 'calibrate':
                client.calibrate(args.values)
                print('Calibration updated in RAM; inspect get-config, then save if appropriate.')
            elif args.action == 'check-standard':
                print(json.dumps(client.check_standard(args.values)))
            elif args.action == 'get-config':
                text = json.dumps(client.get_config(), indent=2, allow_nan=False)
                if args.output:
                    with open(args.output, 'w', encoding='utf-8') as f:
                        f.write(text + '\n')
                else:
                    print(text)
            elif args.action == 'set-config':
                with open(args.file, encoding='utf-8-sig') as f:
                    client.set_config(json.load(f))
                print('Configuration updated in RAM; use command save and check status to persist.')
            elif args.action == 'log':
                print(client.log(args.sequence))
            else:
                v = client.read(10, 10)
                print('state=%d fault=%d step=%d start_result=%d' % tuple(v[:4]))
                print('concentration_mg_l=%s valid=%d' % (float32(v[4], v[5]) if v[6] else 'unavailable', v[6]))
                print('save_status=%d waiting_next=%d preflight_faults=0x%04x' % tuple(v[7:10]))
    except (ImportError, OSError, ValueError, ModbusError) as e:
        print('Error: %s' % e, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
