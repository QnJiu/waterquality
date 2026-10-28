"""Small teaching GUI for the TP Modbus RTU client (requires pyserial)."""
import queue
import threading
import tkinter as tk
from tkinter import ttk

from tp_modbus import Client, float32


def read_status(client):
    """The temperature and status are separate snapshots."""
    v = client.read(10, 10)
    t = client.read(128, 3)
    states = ('空闲', '运行', '完成', '故障')
    state = states[v[0]] if v[0] < len(states) else str(v[0])
    result = '%.3f mg/L' % float32(v[4], v[5]) if v[6] == 1 else '无有效结果'
    temp = '%.2f °C' % float32(t[1], t[2]) if t[0] == 1 else '无有效温度'
    return ('状态：%s  故障码：%d  步骤：%d\n'
            '最近启动结果：%d  浓度：%s\n'
            '保存状态：%d  等待下一步：%s  自检故障位：0x%04X\n'
            '温度：%s') % (state, v[1], v[2], v[3], result, v[7],
                              '是' if v[8] == 1 else '否', v[9], temp)


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


class App:
    def __init__(self, root):
        self.root = root
        root.title('总磷检测教学上位机')
        self.events = queue.Queue()
        self.busy = False
        self.port = tk.StringVar(value='COM3')
        self.baud = tk.StringVar(value='115200')
        self.address = tk.StringVar(value='1')
        self.mode = tk.StringVar(value='受控')
        self.period = tk.StringVar(value='60')
        self.minute = tk.StringVar(value='0')
        self.offset = tk.StringVar(value='0')
        self.message = tk.StringVar(value='填写串口后点击读取状态')
        self.status = tk.StringVar(value='尚未读取')
        self.cal_status = tk.StringVar(value='标定及核查状态尚未读取')
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
                               ('清故障', 3), ('下一步', 5), ('ADC恢复', 8), ('自检', 10)):
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
        ttk.Button(settings, text='设置模式', command=self.set_mode).pack(side='left')
        ttk.Button(settings, text='保存配置', command=lambda: self.start('command', 4)).pack(side='left', padx=2)
        ttk.Label(frame, textvariable=self.message, foreground='blue').grid(sticky='w')
        root.after(100, self.poll)

    def set_mode(self):
        try:
            values = (('受控', '周期', '每日').index(self.mode.get()), int(self.period.get()),
                      int(self.minute.get()), int(self.offset.get()))
        except ValueError:
            self.message.set('模式参数必须为整数'); return
        self.start('schedule', values)

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
        if operation not in ('status', 'cal_status'):
            self.status.set('命令发送后状态可能变化，请重新读取')
            self.cal_status.set('命令发送后标定及核查状态可能变化，请重新读取')
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
                    elif operation == 'schedule':
                        client.schedule(*value)
                        result = '模式已写入内存；需要时点击保存配置并查询保存状态'
                    else:
                        client.command(value)
                        result = '命令已接收，请读取状态确认结果'
                self.events.put((operation, result, None))
            except Exception as error:
                self.events.put((operation, None, str(error)))
        threading.Thread(target=work, daemon=True).start()

    def poll(self):
        try:
            operation, result, error = self.events.get_nowait()
        except queue.Empty:
            pass
        else:
            self.busy = False
            if error:
                if operation == 'status':
                    self.status.set('读取失败，状态未知')
                elif operation == 'cal_status':
                    self.cal_status.set('读取失败，标定及核查状态未知')
                self.message.set('通信失败：' + error)
            elif operation == 'status':
                self.status.set(result)
                self.message.set('状态已读取；温度与状态为分别读取的快照')
            elif operation == 'cal_status':
                self.cal_status.set(result)
                self.message.set('标定和核查状态为分别读取的快照')
            else:
                self.message.set(result)
        self.root.after(100, self.poll)


if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
