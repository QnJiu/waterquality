"""Small teaching GUI for the TP Modbus RTU client (requires pyserial)."""
import math
import queue
from datetime import datetime, timezone
import time
import threading
import tkinter as tk
from tkinter import ttk

from tp_modbus import Client, FLOAT_FIELDS, ModbusError, float32


def read_status(client):
    """Status, temperature and signal readings are separate snapshots."""
    v = client.read(10, 10)
    t = client.read(128, 3)
    maintenance = client.maintenance_status()
    try:
        readings = client.readings()
        signal = '  '.join('%s：%s' % (label, '无' if readings[name] is None else '%.4f' % readings[name])
                           for name, label in (('blank', '空白'), ('reacted', '反应后'), ('turbidity', '浊度')))
    except ModbusError as error:
        if str(error) != 'device exception 2':
            raise
        signal = '设备不支持采样信号查询'
    states = ('空闲', '运行', '完成', '故障')
    state = states[v[0]] if v[0] < len(states) else str(v[0])
    result = '%.3f mg/L' % float32(v[4], v[5]) if v[6] == 1 else '无有效结果'
    temp = '%.2f °C' % float32(t[1], t[2]) if t[0] == 1 else '无有效温度'
    return ('状态：%s  故障码：%d  步骤：%d\n'
            '最近启动结果：%d  浓度：%s\n'
            '保存状态：%d  等待下一步：%s  自检故障位：0x%04X\n'
            '温度：%s  升级维护：%s\n采样信号：%s') % (state, v[1], v[2], v[3], result, v[7],
                              '是' if v[8] == 1 else '否', v[9], temp,
                              '未知' if maintenance is None else '是' if maintenance else '否', signal)


def read_calibration_status(client):
    """Read the two independent automatic workflow snapshots."""
    cal = client.auto_calibration_status()
    standard = client.auto_standard_status()
    cal_names = {'unavailable': '未配置', 'idle': '空闲', 'low': '低点采样',
                 'high': '高点采样', 'complete': '完成', 'failed': '失败',
                 'cancelled': '已取消'}
    std_names = {'unavailable': '未配置', 'idle': '空闲', 'running': '运行中',
                 'complete': '完成', 'failed': '失败', 'cancelled': '已取消'}
    actual = standard['actual_mg_l']
    outcome = ('通过' if standard['passed'] else '不通过') if standard['passed'] is not None else '无结果'
    concentration = '%.3f mg/L' % actual if actual is not None else '无'
    return ('自动标定：%s  错误码：%d\n标液核查：%s  错误码：%d  结果：%s  浓度：%s' %
            (cal_names.get(cal['state'], '未知'), cal['error_code'],
             std_names.get(standard['state'], '未知'), standard['error'], outcome, concentration))


def read_clock_status(client):
    snapshot = client.clock_status()
    names = {'unavailable': '未配置', 'idle': '空闲', 'pending': '校时中',
             'verified': '校时回读成功', 'failed': '校时失败'}
    seconds = snapshot['utc_seconds']
    clock = datetime.fromtimestamp(seconds, timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC') if seconds is not None else '无有效时间'
    return '时钟：%s  %s' % (names.get(snapshot['state'], '未知'), clock)


def read_event(client, sequence=None):
    event = client.log(sequence)
    timestamp = str(event['utc']) + ' UTC秒' if event['utc'] is not None else '无时间'
    text = ('序号：%d  类型：%d  详情：%d  时间：%s  原始值：0x%08X' %
            (event['sequence'], event['type'], event['detail'], timestamp, event['value_bits']))
    if event['type'] == 2 and event['detail'] == 1:
        value = event['value_bits']
        concentration = float32(value >> 16, value & 0xffff)
        text += '  浓度：%.3f mg/L' % concentration if math.isfinite(concentration) else '  浓度无效'
    return text


READ_OPERATIONS = ('status', 'cal_status', 'clock_status', 'event', 'config_read', 'schedule_read')


CONFIG_LABELS = {
    'slope': '标定斜率', 'intercept': '标定截距', 'turbidity_factor': '浊度系数',
    'min_signal': '最小信号', 'max_concentration': '浓度上限 (mg/L)',
    'kp': 'PID 比例', 'ki': 'PID 积分', 'kd': 'PID 微分', 'deadband_c': '温度死区 (°C)',
    'output_max': '输出上限', 'integral_max': '积分上限', 'max_temperature_c': '温度上限 (°C)',
    'period_seconds': '周期 (秒)', 'slave_address': '从站地址',
}


def parse_config(values):
    """Convert editable strings; the client and device validate numeric ranges."""
    return {name: (float(values[name]) if name in FLOAT_FIELDS else int(values[name]))
            for name in CONFIG_LABELS}


class App:
    def __init__(self, root):
        self.root = root
        root.title('总磷检测教学上位机')
        self.events = queue.Queue()
        self.busy = False
        self.config_window = None
        self.port = tk.StringVar(value='COM3')
        self.baud = tk.StringVar(value='115200')
        self.address = tk.StringVar(value='1')
        self.mode = tk.StringVar(value='受控')
        self.period = tk.StringVar(value='60')
        self.minute = tk.StringVar(value='0')
        self.offset = tk.StringVar(value='0')
        self.message = tk.StringVar(value='填写串口后读取状态和模式；模式输入框初始值不是设备配置')
        self.status = tk.StringVar(value='尚未读取')
        self.cal_status = tk.StringVar(value='标定及核查状态尚未读取')
        self.clock_status = tk.StringVar(value='时钟状态尚未读取')
        self.event_status = tk.StringVar(value='事件尚未读取')
        self.event_sequence = tk.StringVar(value='')
        self.connection_generation = 0
        for variable in (self.port, self.baud, self.address):
            variable.trace_add('write', self.connection_changed)
        frame = ttk.Frame(root, padding=12)
        frame.grid(sticky='nsew')
        row = ttk.Frame(frame); row.grid(sticky='w')
        for label, var, width in (('串口', self.port, 10), ('波特率', self.baud, 8),
                                  ('从站地址', self.address, 4)):
            ttk.Label(row, text=label).pack(side='left')
            ttk.Entry(row, textvariable=var, width=width).pack(side='left', padx=4)
        ttk.Label(frame, textvariable=self.status, justify='left').grid(sticky='w', pady=10)
        controls = ttk.Frame(frame); controls.grid(sticky='w')
        for label, command in (('读取状态', None), ('启动', 1), ('停止', 2),
                               ('清故障', 3), ('下一步', 5), ('ADC恢复', 8), ('自检', 10), ('故障恢复', 11)):
            ttk.Button(controls, text=label, command=lambda c=command: self.start('status' if c is None else 'command', c)).pack(side='left', padx=2)
        calibration = ttk.Frame(frame); calibration.grid(sticky='w', pady=(8, 0))
        for label, operation, command in (('读取标定状态', 'cal_status', None),
                                          ('启动自动标定', 'command', 6),
                                          ('应用标定', 'command', 7),
                                          ('启动标液核查', 'command', 9)):
            ttk.Button(calibration, text=label,
                       command=lambda op=operation, c=command: self.start(op, c)).pack(side='left', padx=2)
        ttk.Label(frame, textvariable=self.cal_status, justify='left').grid(sticky='w', pady=(4, 0))
        settings = ttk.Frame(frame); settings.grid(sticky='w', pady=10)
        ttk.Combobox(settings, textvariable=self.mode, values=('受控', '周期', '每日'),
                     state='readonly', width=5).pack(side='left')
        for label, var, width in (('周期秒', self.period, 7), ('每日分钟', self.minute, 5),
                                  ('UTC偏移分钟', self.offset, 5)):
            ttk.Label(settings, text=label).pack(side='left')
            ttk.Entry(settings, textvariable=var, width=width).pack(side='left', padx=3)
        ttk.Button(settings, text='读取模式', command=lambda: self.start('schedule_read')).pack(side='left')
        ttk.Button(settings, text='设置模式', command=self.set_mode).pack(side='left')
        ttk.Button(settings, text='读取参数', command=lambda: self.start('config_read')).pack(side='left', padx=2)
        ttk.Button(settings, text='保存配置', command=lambda: self.start('command', 4)).pack(side='left', padx=2)
        clock = ttk.Frame(frame); clock.grid(sticky='w')
        ttk.Button(clock, text='读取时钟', command=lambda: self.start('clock_status')).pack(side='left')
        ttk.Button(clock, text='用电脑时间校时', command=lambda: self.start('set_clock')).pack(side='left', padx=4)
        ttk.Label(clock, textvariable=self.clock_status).pack(side='left', padx=4)
        event = ttk.Frame(frame); event.grid(sticky='w', pady=(8, 0))
        ttk.Label(event, text='事件序号（留空读取最新）').pack(side='left')
        ttk.Entry(event, textvariable=self.event_sequence, width=10).pack(side='left', padx=4)
        ttk.Button(event, text='读取事件', command=self.read_event).pack(side='left')
        ttk.Label(event, textvariable=self.event_status).pack(side='left', padx=4)
        ttk.Label(frame, textvariable=self.message, foreground='blue').grid(sticky='w')
        root.after(100, self.poll)

    def connection_changed(self, *_):
        self.connection_generation += 1
        self.status.set('连接已改变，请重新读取状态')
        self.cal_status.set('连接已改变，请重新读取标定及核查状态')
        self.clock_status.set('连接已改变，请重新读取时钟')
        self.event_status.set('连接已改变，请重新读取事件')
        self.message.set('连接已改变，模式输入框不代表当前设备；请重新读取')

    def read_event(self):
        text = self.event_sequence.get().strip()
        try:
            sequence = int(text) if text else None
            if sequence is not None and not 0 <= sequence <= 0xffffffff:
                raise ValueError
        except ValueError:
            self.message.set('事件序号须为 0 到 4294967295 的整数'); return
        self.start('event', sequence)

    def set_mode(self):
        try:
            values = (('受控', '周期', '每日').index(self.mode.get()), int(self.period.get()),
                      int(self.minute.get()), int(self.offset.get()))
        except ValueError:
            self.message.set('模式参数必须为整数'); return
        self.start('schedule', values)

    def show_config(self, config, target):
        if self.config_window is not None and self.config_window.winfo_exists():
            self.config_window.destroy()
        window = self.config_window = tk.Toplevel(self.root)
        window.title('标定和温控参数')
        ttk.Label(window, text='读取时的完整快照，写入会覆盖整组参数；请避免其他上位机同时修改。').grid(row=0, columnspan=2, padx=8, pady=6)
        entries = {}
        for row, (name, label) in enumerate(CONFIG_LABELS.items(), 1):
            ttk.Label(window, text=label + ' (' + name + ')').grid(row=row, column=0, sticky='w', padx=8)
            entry = ttk.Entry(window, width=20)
            entry.insert(0, str(config[name]))
            entry.grid(row=row, column=1, padx=8, pady=2)
            entries[name] = entry

        def write_config():
            if (self.port.get().strip(), self.baud.get(), self.address.get()) != target:
                self.message.set('连接参数已改变，请重新读取参数'); return
            try:
                values = parse_config({name: entry.get() for name, entry in entries.items()})
            except ValueError:
                self.message.set('参数格式错误：周期和地址须为整数，其他项须为数字'); return
            self.start('config_write', values)

        ttk.Button(window, text='写入参数', command=write_config).grid(columnspan=2, pady=6)
        ttk.Label(window, textvariable=self.message, foreground='blue', wraplength=480).grid(columnspan=2, padx=8, pady=4)

    def start(self, operation, value=None):
        if self.busy:
            self.message.set('通信中，请稍候'); return
        try:
            port, baud, address = self.port.get().strip(), int(self.baud.get()), int(self.address.get())
            if not port or baud <= 0 or not 1 <= address <= 247:
                raise ValueError('串口、波特率或地址无效')
        except ValueError as error:
            self.message.set(str(error)); return
        self.busy = True
        generation = self.connection_generation
        if operation not in READ_OPERATIONS:
            self.status.set('命令发送后状态可能变化，请重新读取')
            self.cal_status.set('命令发送后标定及核查状态可能变化，请重新读取')
            self.clock_status.set('时间或状态可能变化，请重新读取时钟')
        self.message.set('通信中…')
        def work():
            try:
                import serial
                with serial.Serial(port, baud, bytesize=8, parity='N', stopbits=1,
                                   timeout=0.1, write_timeout=2) as connection:
                    client = Client(connection, address, 2)
                    if operation == 'status':
                        result = read_status(client)
                    elif operation == 'cal_status':
                        result = read_calibration_status(client)
                    elif operation == 'clock_status':
                        result = read_clock_status(client)
                    elif operation == 'event':
                        result = read_event(client, value)
                    elif operation == 'schedule_read':
                        result = client.get_schedule()
                    elif operation == 'config_read':
                        result = (client.get_config(), (port, str(baud), str(address)))
                    elif operation == 'config_write':
                        client.set_config(value)
                        result = '参数已写入内存；请保存配置并读取状态确认，地址更改重启后生效'
                    elif operation == 'set_clock':
                        client.set_clock(int(time.time()))
                        result = '校时请求已接收，请读取时钟确认回读成功'
                    elif operation == 'schedule':
                        client.schedule(*value)
                        result = '模式已写入内存；需要时点击保存配置并查询保存状态'
                    else:
                        client.command(value)
                        result = '命令已接收，请读取状态确认结果'
                self.events.put((generation, port, address, operation, result, None))
            except Exception as error:
                self.events.put((generation, port, address, operation, None, str(error)))
        threading.Thread(target=work, daemon=True).start()

    def poll(self):
        try:
            generation, port, address, operation, result, error = self.events.get_nowait()
        except queue.Empty:
            pass
        else:
            self.busy = False
            if generation != self.connection_generation:
                if operation in READ_OPERATIONS:
                    self.message.set('连接已改变，已丢弃旧设备的读取结果；请重新读取')
                else:
                    detail = '通信失败，执行结果可能未知：' + error if error else result
                    self.message.set('上次操作目标为 %s 地址 %s：%s；当前连接已改变' % (port, address, detail))
            elif error:
                if operation == 'status':
                    self.status.set('读取失败，状态未知')
                elif operation == 'cal_status':
                    self.cal_status.set('读取失败，标定及核查状态未知')
                elif operation == 'clock_status':
                    self.clock_status.set('读取失败，时钟状态未知')
                elif operation == 'event':
                    self.event_status.set('读取失败，事件未知')
                if operation in READ_OPERATIONS:
                    self.message.set('通信失败：' + error)
                else:
                    self.message.set('通信失败，执行结果可能未知，请读取设备状态确认：' + error)
            elif operation == 'status':
                self.status.set(result)
                self.message.set('状态已读取；温度与状态为分别读取的快照')
            elif operation == 'cal_status':
                self.cal_status.set(result)
                self.message.set('标定和核查状态为分别读取的快照')
            elif operation == 'clock_status':
                self.clock_status.set(result)
                self.message.set('时钟快照已读取')
            elif operation == 'schedule_read':
                mode, period, minute, offset = result
                self.mode.set(('受控', '周期', '每日')[mode])
                self.period.set(str(period))
                self.minute.set(str(minute))
                self.offset.set(str(offset))
                self.message.set('已读取设备模式快照；修改后需设置并保存配置')
            elif operation == 'config_read':
                self.show_config(*result)
                self.message.set('参数已读取；写入后需保存并确认保存状态')
            elif operation == 'event':
                self.event_status.set(result)
                self.message.set('事件已读取')
            else:
                self.message.set(result)
        self.root.after(100, self.poll)


if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
