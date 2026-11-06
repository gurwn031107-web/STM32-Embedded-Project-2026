"""
elevator_gui.py - PC control panel for the STM32 3-floor elevator (UART2, 115200 8N1)

Run:   pip install pyside6 pyserial
       python elevator_gui.py
Port "DEMO" = virtual board (fake_mcu.py), works without hardware.

Protocol (one line each, '\n' terminated)
  PC -> board : CALL 1 | CALL 2 | CALL 3 | STOP | HOME
  board -> PC : log text (shown as-is) and
                $ST,<state>,<cur>,<target>,<dir>,<pos>,<door>,<s1s2s3>,<err>
                  state : HOMING / IDLE / MOVING / DOOR / ERROR
                  cur, target : 0~3 (0 = unknown)
                  dir   : U / D / S
                  pos   : step counter (1F = 0)
                  door  : 0 closed, 1 arriving(ding-dong), 2 open
                  s1s2s3: floor sensors, 1 = blocked  (e.g. 010)
                  err   : NONE / TIMEOUT / SENSOR / STOP / NOHOME

Keys: 1 / 2 / 3 = call,  Space = STOP,  H = HOME
"""

import sys
import time
from dataclasses import dataclass, field

import serial
import serial.tools.list_ports
from PySide6.QtCore import Qt, QTimer, QRectF, QPointF
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QBrush, QPolygonF, QTextCursor
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QGridLayout,
    QPushButton, QComboBox, QLabel, QGroupBox, QPlainTextEdit, QLineEdit,
    QCheckBox, QFileDialog, QSplitter, QFrame,
)

from fake_mcu import FakeMcu

DEMO_PORT = "DEMO (가상 보드)"
BAUDS = ["115200", "57600", "38400", "19200", "9600"]
LINK_TIMEOUT_S = 2.0
DEFAULT_FLOOR_SPAN = 1800      # steps per floor, used until the real value is learned

# colors
BG = "#11161d"
PANEL = "#1a222c"
EDGE = "#2c3846"
TEXT = "#e8edf3"
SUB = "#aab6c4"
ACCENT = "#3d9bff"
OK = "#2ecc71"
WARN = "#f5b041"
ERR = "#ff5c5c"

STATE_KR = {
    "HOMING": ("원점 복귀 중", WARN),
    "IDLE": ("대기", OK),
    "MOVING": ("이동 중", ACCENT),
    "DOOR": ("도착 / 문 동작", OK),
    "ERROR": ("정지 (에러)", ERR),
}
ERR_KR = {
    "NONE": "없음",
    "TIMEOUT": "TIMEOUT - 목표 층 센서 미감지",
    "SENSOR": "SENSOR - 센서 이상",
    "STOP": "비상 정지",
    "NOHOME": "위치 모름 - 원점 필요",
}
DOOR_KR = {0: "닫힘", 1: "도착 알림", 2: "열림"}


@dataclass
class Status:
    valid: bool = False
    state: str = "-"
    cur: int = 0
    target: int = 0
    dir: str = "S"
    pos: int = 0
    door: int = 0
    sens: list = field(default_factory=lambda: [False, False, False])
    err: str = "NONE"

    @staticmethod
    def parse(line):
        p = line.split(",")
        if len(p) != 9 or p[0] != "$ST":
            return None
        try:
            return Status(True, p[1], int(p[2]), int(p[3]), p[4], int(p[5]), int(p[6]),
                          [c == "1" for c in p[7][:3].ljust(3, "0")], p[8])
        except ValueError:
            return None


# ---------------------------------------------------------------- shaft view
class ShaftView(QWidget):
    """Draws the 3-floor shaft, the car, its doors and the floor sensors."""

    def __init__(self):
        super().__init__()
        self.setMinimumSize(300, 480)
        self.st = Status()
        self.floor_pos = {1: 0}       # learned step position of each floor
        self.level = 1.0              # displayed car level (1.0 = 1F, 3.0 = 3F)
        self.level_goal = 1.0
        self.door = 0.0               # displayed door opening 0..1
        self.home_ref = None          # (pos, level) at start of homing
        self.blink = False
        self._t = QTimer(self, interval=16, timeout=self._animate)
        self._t.start()
        self._bt = QTimer(self, interval=400, timeout=self._toggle_blink)
        self._bt.start()

    def _toggle_blink(self):
        self.blink = not self.blink

    def _span(self):
        known = [(f, p) for f, p in self.floor_pos.items() if f > 1 and p > 0]
        if known:
            return sum(p / (f - 1) for f, p in known) / len(known)
        return DEFAULT_FLOOR_SPAN

    def _pos_to_level(self, pos):
        pts = {f: self.floor_pos.get(f, (f - 1) * self._span()) for f in (1, 2, 3)}
        if pos <= pts[1]:
            return 1.0 + (pos - pts[1]) / self._span()
        for f in (1, 2):
            a, b = pts[f], pts[f + 1]
            if pos <= b and b > a:
                return f + (pos - a) / (b - a)
        return 3.0 + (pos - pts[3]) / self._span()

    def set_status(self, st):
        prev = self.st
        self.st = st
        if st.state in ("IDLE", "DOOR") and st.cur and st.sens[st.cur - 1]:
            self.floor_pos[st.cur] = st.pos            # learn where this floor is
            self.level_goal = float(st.cur)
            self.home_ref = None
        elif st.state == "HOMING":
            if prev.state != "HOMING" or self.home_ref is None:
                self.home_ref = (st.pos, self.level if prev.valid else (st.cur or 2.5))
            ref_pos, ref_lv = self.home_ref
            self.level_goal = ref_lv - (ref_pos - st.pos) / self._span()
        else:
            self.level_goal = self._pos_to_level(st.pos)
            if st.state == "MOVING" and st.target:     # do not draw past the target before arriving
                if st.dir == "U":
                    self.level_goal = min(self.level_goal, st.target - 0.02)
                elif st.dir == "D":
                    self.level_goal = max(self.level_goal, st.target + 0.02)
        self.level_goal = max(0.9, min(3.1, self.level_goal))
        if not prev.valid:
            self.level = self.level_goal
        self.update()

    def _animate(self):
        goal_door = 1.0 if self.st.door == 2 else 0.0
        changed = abs(self.level - self.level_goal) > 1e-3 or abs(self.door - goal_door) > 1e-3
        self.level += (self.level_goal - self.level) * 0.2
        self.door += max(-0.04, min(0.04, goal_door - self.door))
        if changed or self.st.state in ("ERROR", "HOMING"):
            self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        p.fillRect(self.rect(), QColor(BG))

        # floor indicator on top
        ind = QRectF(w * 0.18, 10, w * 0.64, 60)
        p.setPen(QPen(QColor(EDGE), 2))
        p.setBrush(QColor("#05080b"))
        p.drawRoundedRect(ind, 8, 8)
        st = self.st
        txt = f"{st.cur}F" if st.valid and st.cur else "--"
        color = QColor(ERR) if st.state == "ERROR" else QColor("#ffb020")
        p.setPen(color)
        p.setFont(QFont("Consolas", 28, QFont.Bold))
        p.drawText(ind.adjusted(0, 0, -ind.width() * 0.3, 0), Qt.AlignCenter, txt)
        if st.dir in ("U", "D"):
            cx, cy, s = ind.right() - ind.width() * 0.22, ind.center().y(), 14
            tri = [QPointF(cx, cy - s), QPointF(cx - s, cy + s * 0.7), QPointF(cx + s, cy + s * 0.7)] \
                if st.dir == "U" else \
                [QPointF(cx, cy + s), QPointF(cx - s, cy - s * 0.7), QPointF(cx + s, cy - s * 0.7)]
            p.setBrush(color)
            p.setPen(Qt.NoPen)
            p.drawPolygon(QPolygonF(tri))

        # shaft
        top, bottom = 90, h - 20
        slot = (bottom - top) / 3
        shaft = QRectF(w * 0.30, top, w * 0.40, bottom - top)
        p.setPen(QPen(QColor(EDGE), 2))
        p.setBrush(QColor(PANEL))
        p.drawRect(shaft)

        def floor_y(level):          # y of the floor line for a level (1.0 .. 3.0)
            return bottom - (level - 1.0) * slot

        p.setFont(QFont("Malgun Gothic", 13, QFont.Bold))
        for f in (1, 2, 3):
            y = floor_y(f)
            p.setPen(QPen(QColor(EDGE), 2))
            p.drawLine(QPointF(w * 0.08, y), QPointF(w * 0.92, y))
            p.setPen(QColor(TEXT))
            p.drawText(QRectF(w * 0.04, y - slot, w * 0.22, slot), Qt.AlignVCenter | Qt.AlignLeft, f"{f}F")
            # sensor LED on the right wall
            blocked = st.sens[f - 1]
            c = QColor(WARN) if blocked else QColor("#33404d")
            p.setPen(QPen(QColor("#000000"), 1))
            p.setBrush(c)
            sy = y - slot * 0.5
            p.drawEllipse(QPointF(w * 0.78, sy), 9, 9)
            p.setPen(QColor(SUB))
            p.setFont(QFont("Malgun Gothic", 9))
            p.drawText(QRectF(w * 0.78 + 14, sy - 10, w * 0.2, 20), Qt.AlignVCenter | Qt.AlignLeft,
                       "센서 ON" if blocked else "센서")
            p.setFont(QFont("Malgun Gothic", 13, QFont.Bold))

        # target marker
        if st.valid and st.target and st.state in ("MOVING", "HOMING"):
            ty = floor_y(st.target) - slot * 0.5
            p.setPen(Qt.NoPen)
            p.setBrush(QColor(ACCENT))
            x0 = shaft.left() - 6
            p.drawPolygon(QPolygonF([QPointF(x0, ty), QPointF(x0 - 14, ty - 9), QPointF(x0 - 14, ty + 9)]))

        # car
        ch = slot * 0.78
        cw = shaft.width() * 0.78
        cy = floor_y(self.level) - ch - 2
        car = QRectF(shaft.center().x() - cw / 2, cy, cw, ch)
        frame = QColor(ERR) if st.state == "ERROR" and self.blink else QColor("#c9d3de")
        p.setPen(QPen(frame, 3))
        p.setBrush(QColor("#2a3542"))
        p.drawRect(car)
        # doors (slide open to both sides)
        gap = (cw / 2) * self.door * 0.9
        inner = car.adjusted(4, 4, -4, -4)
        half = inner.width() / 2
        p.setPen(QPen(QColor("#5f6f80"), 1))
        p.setBrush(QColor("#8a9aab"))
        p.drawRect(QRectF(inner.left(), inner.top(), half - gap, inner.height()))
        p.drawRect(QRectF(inner.center().x() + gap, inner.top(), half - gap, inner.height()))
        if self.door > 0.05:
            p.setBrush(QColor("#ffe9a8"))
            p.setPen(Qt.NoPen)
            p.drawRect(QRectF(inner.center().x() - gap, inner.top(), gap * 2, inner.height()))

        if not st.valid:
            p.setPen(QColor(SUB))
            p.setFont(QFont("Malgun Gothic", 11))
            p.drawText(shaft, Qt.AlignCenter, "상태 수신 대기")
        p.end()


# ---------------------------------------------------------------- main window
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Elevator Control")
        self.resize(1180, 760)
        self.ser = None
        self.rxbuf = b""
        self.last_rx = 0.0
        self.st = Status()

        root = QWidget()
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        outer.setContentsMargins(12, 12, 12, 12)
        outer.setSpacing(10)

        outer.addWidget(self._build_conn_bar())

        split = QSplitter(Qt.Horizontal)
        self.shaft = ShaftView()
        split.addWidget(self._framed(self.shaft))
        split.addWidget(self._build_control())
        split.addWidget(self._build_log())
        split.setSizes([330, 330, 520])
        outer.addWidget(split, 1)

        self.poll = QTimer(self, interval=20, timeout=self._poll_serial)
        self.link = QTimer(self, interval=300, timeout=self._check_link)
        self.link.start()
        self._refresh_ports()
        self._set_connected(False)

    # ---------- UI building ----------
    def _framed(self, w):
        f = QFrame()
        f.setObjectName("card")
        l = QVBoxLayout(f)
        l.setContentsMargins(6, 6, 6, 6)
        l.addWidget(w)
        return f

    def _build_conn_bar(self):
        bar = QFrame()
        bar.setObjectName("card")
        l = QHBoxLayout(bar)
        l.setContentsMargins(12, 8, 12, 8)
        title = QLabel("STM32 Elevator")
        title.setObjectName("title")
        l.addWidget(title)
        l.addSpacing(20)
        l.addWidget(QLabel("포트"))
        self.cb_port = QComboBox()
        self.cb_port.setMinimumWidth(220)
        l.addWidget(self.cb_port)
        b = QPushButton("새로고침")
        b.clicked.connect(self._refresh_ports)
        l.addWidget(b)
        l.addWidget(QLabel("Baud"))
        self.cb_baud = QComboBox()
        self.cb_baud.addItems(BAUDS)
        l.addWidget(self.cb_baud)
        self.btn_conn = QPushButton("연결")
        self.btn_conn.setObjectName("primary")
        self.btn_conn.clicked.connect(self._toggle_conn)
        l.addWidget(self.btn_conn)
        l.addStretch(1)
        self.lb_link = QLabel()
        l.addWidget(self.lb_link)
        return bar

    def _build_control(self):
        box = QFrame()
        box.setObjectName("card")
        l = QVBoxLayout(box)
        l.setContentsMargins(14, 14, 14, 14)
        l.setSpacing(12)

        g = QGroupBox("층 호출")
        gl = QVBoxLayout(g)
        self.btn_floor = {}
        for f in (3, 2, 1):
            b = QPushButton(f"{f}F")
            b.setObjectName("floor")
            b.setFocusPolicy(Qt.NoFocus)
            b.setMinimumHeight(54)
            b.clicked.connect(lambda _, f=f: self.send(f"CALL {f}"))
            gl.addWidget(b)
            self.btn_floor[f] = b
        l.addWidget(g)

        row = QHBoxLayout()
        self.btn_stop = QPushButton("비상 정지")
        self.btn_stop.setObjectName("stop")
        self.btn_stop.setMinimumHeight(54)
        self.btn_stop.setFocusPolicy(Qt.NoFocus)
        self.btn_stop.clicked.connect(lambda: self.send("STOP"))
        self.btn_home = QPushButton("원점 복귀")
        self.btn_home.setMinimumHeight(54)
        self.btn_home.setFocusPolicy(Qt.NoFocus)
        self.btn_home.clicked.connect(lambda: self.send("HOME"))
        row.addWidget(self.btn_stop, 3)
        row.addWidget(self.btn_home, 2)
        l.addLayout(row)

        g2 = QGroupBox("상태")
        grid = QGridLayout(g2)
        grid.setVerticalSpacing(8)
        self.val = {}
        for r, (k, name) in enumerate([("state", "상태"), ("cur", "현재 층"), ("target", "목표 층"),
                                        ("pos", "위치 (step)"), ("door", "문"), ("err", "에러")]):
            a = QLabel(name)
            a.setObjectName("key")
            v = QLabel("-")
            v.setObjectName("val")
            v.setWordWrap(True)
            grid.addWidget(a, r, 0)
            grid.addWidget(v, r, 1)
            self.val[k] = v
        grid.setColumnStretch(1, 1)
        l.addWidget(g2)

        hint = QLabel("단축키  1 / 2 / 3 호출 · Space 정지 · H 원점")
        hint.setObjectName("hint")
        l.addWidget(hint)
        l.addStretch(1)
        return box

    def _build_log(self):
        box = QFrame()
        box.setObjectName("card")
        l = QVBoxLayout(box)
        l.setContentsMargins(10, 10, 10, 10)
        top = QHBoxLayout()
        lb = QLabel("UART 로그")
        lb.setObjectName("section")
        top.addWidget(lb)
        top.addStretch(1)
        self.chk_st = QCheckBox("$ST 상태줄 표시")
        self.chk_st.setFocusPolicy(Qt.NoFocus)
        top.addWidget(self.chk_st)
        self.chk_pos = QCheckBox("UP/DOWN pos 줄 숨김")
        self.chk_pos.setFocusPolicy(Qt.NoFocus)
        top.addWidget(self.chk_pos)
        for name, fn in (("지우기", lambda: self.log.clear()), ("저장", self._save_log)):
            b = QPushButton(name)
            b.setFocusPolicy(Qt.NoFocus)
            b.clicked.connect(fn)
            top.addWidget(b)
        l.addLayout(top)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(3000)
        self.log.setFocusPolicy(Qt.ClickFocus)
        l.addWidget(self.log, 1)

        row = QHBoxLayout()
        self.ed_cmd = QLineEdit()
        self.ed_cmd.setPlaceholderText("직접 명령 (예: CALL 2)  Enter로 전송")
        self.ed_cmd.returnPressed.connect(self._send_manual)
        row.addWidget(self.ed_cmd, 1)
        b = QPushButton("전송")
        b.clicked.connect(self._send_manual)
        row.addWidget(b)
        l.addLayout(row)
        return box

    # ---------- serial ----------
    def _refresh_ports(self):
        cur = self.cb_port.currentText()
        self.cb_port.clear()
        for p in serial.tools.list_ports.comports():
            label = f"{p.device}  {p.description}" if p.description and p.description != "n/a" else p.device
            self.cb_port.addItem(label, p.device)
            if "STLink" in (p.description or "") or "STM" in (p.description or ""):
                self.cb_port.setCurrentIndex(self.cb_port.count() - 1)   # pick the Nucleo port
        self.cb_port.addItem(DEMO_PORT, "DEMO")
        i = self.cb_port.findText(cur)
        if i >= 0:
            self.cb_port.setCurrentIndex(i)

    def _toggle_conn(self):
        if self.ser:
            self._close()
            return
        dev = self.cb_port.currentData()
        try:
            if dev == "DEMO":
                self.ser = FakeMcu()
            else:
                self.ser = serial.Serial(dev, int(self.cb_baud.currentText()), timeout=0)
        except (serial.SerialException, OSError) as e:
            self._log(f"[연결 실패] {e}", ERR)
            self.ser = None
            return
        self.rxbuf = b""
        self.last_rx = 0.0
        self._log(f"[연결] {self.cb_port.currentText()} @ {self.cb_baud.currentText()}", ACCENT)
        self.poll.start()
        self._set_connected(True)

    def _close(self):
        self.poll.stop()
        try:
            self.ser.close()
        except Exception:
            pass
        self.ser = None
        self._log("[연결 해제]", SUB)
        self.st = Status()
        self.shaft.set_status(self.st)
        self._update_status_panel()
        self._set_connected(False)

    def _set_connected(self, on):
        self.btn_conn.setText("연결 해제" if on else "연결")
        self.cb_port.setEnabled(not on)
        self.cb_baud.setEnabled(not on)
        for b in list(self.btn_floor.values()) + [self.btn_stop, self.btn_home]:
            b.setEnabled(on)
        self._check_link()

    def _poll_serial(self):
        if not self.ser:
            return
        try:
            n = self.ser.in_waiting
            data = self.ser.read(n) if n else b""
        except (serial.SerialException, OSError) as e:
            self._log(f"[포트 오류] {e}", ERR)
            self._close()
            return
        if not data:
            return
        self.last_rx = time.monotonic()
        self.rxbuf += data
        while b"\n" in self.rxbuf:
            raw, self.rxbuf = self.rxbuf.split(b"\n", 1)
            self._on_line(raw.decode("ascii", errors="replace").strip("\r"))

    def _on_line(self, line):
        if line.startswith("$ST"):
            st = Status.parse(line)
            if st:
                self.st = st
                self.shaft.set_status(st)
                self._update_status_panel()
            if self.chk_st.isChecked():
                self._log(line, SUB)
            return
        u = line.upper()
        if self.chk_pos.isChecked() and (u.startswith("UP ") or u.startswith("DOWN ")):
            return
        color = ERR if ("ERROR" in u or "EMERGENCY" in u) else \
            WARN if ("WARN" in u or "BUSY" in u or u.startswith("?")) else \
            OK if ("ARRIVED" in u or "HOME OK" in u or "DOOR" in u) else TEXT
        self._log(line, color)

    def send(self, cmd):
        if not self.ser:
            return
        try:
            self.ser.write((cmd + "\r\n").encode("ascii"))
        except (serial.SerialException, OSError) as e:
            self._log(f"[전송 실패] {e}", ERR)
            return
        self._log(f">> {cmd}", ACCENT)

    def _send_manual(self):
        t = self.ed_cmd.text().strip()
        if t:
            self.send(t.upper())
            self.ed_cmd.clear()

    def _check_link(self):
        if not self.ser:
            self.lb_link.setText("●  연결 안 됨")
            self.lb_link.setStyleSheet(f"color:{SUB}; font-weight:bold;")
        elif time.monotonic() - self.last_rx > LINK_TIMEOUT_S:
            self.lb_link.setText("●  연결됨 · 수신 없음 (보드 리셋/배선 확인)")
            self.lb_link.setStyleSheet(f"color:{WARN}; font-weight:bold;")
        elif not self.st.valid:
            self.lb_link.setText("●  수신 중 · $ST 상태줄 없음")
            self.lb_link.setStyleSheet(f"color:{WARN}; font-weight:bold;")
        else:
            self.lb_link.setText("●  통신 정상")
            self.lb_link.setStyleSheet(f"color:{OK}; font-weight:bold;")

    # ---------- view ----------
    def _update_status_panel(self):
        st = self.st
        if not st.valid:
            for v in self.val.values():
                v.setText("-")
                v.setStyleSheet("")
            for b in self.btn_floor.values():
                b.setProperty("lit", False)
                b.style().polish(b)
            return
        name, color = STATE_KR.get(st.state, (st.state, TEXT))
        self.val["state"].setText(name)
        self.val["state"].setStyleSheet(f"color:{color};")
        self.val["cur"].setText(f"{st.cur}F" if st.cur else "모름")
        self.val["target"].setText(f"{st.target}F" if st.target and st.state in ("MOVING", "HOMING") else "-")
        self.val["pos"].setText(str(st.pos))
        self.val["door"].setText(DOOR_KR.get(st.door, str(st.door)))
        self.val["err"].setText(ERR_KR.get(st.err, st.err))
        self.val["err"].setStyleSheet(f"color:{ERR if st.err != 'NONE' else TEXT};")
        for f, b in self.btn_floor.items():
            lit = st.state in ("MOVING", "HOMING") and st.target == f
            if b.property("lit") != lit:
                b.setProperty("lit", lit)
                b.style().polish(b)
        self._check_link()

    def _log(self, text, color=TEXT):
        t = time.strftime("%H:%M:%S")
        safe = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        self.log.appendHtml(f'<span style="color:{SUB}">{t}</span>&nbsp; '
                            f'<span style="color:{color}">{safe}</span>')
        self.log.moveCursor(QTextCursor.End)

    def _save_log(self):
        path, _ = QFileDialog.getSaveFileName(self, "로그 저장",
                                              time.strftime("elevator_log_%Y%m%d_%H%M%S.txt"),
                                              "Text (*.txt)")
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.log.toPlainText())

    # ---------- keys ----------
    def keyPressEvent(self, e):
        k = e.key()
        if k in (Qt.Key_1, Qt.Key_2, Qt.Key_3):
            self.send(f"CALL {k - Qt.Key_0}")
        elif k == Qt.Key_Space:
            self.send("STOP")
        elif k == Qt.Key_H:
            self.send("HOME")
        else:
            super().keyPressEvent(e)

    def closeEvent(self, e):
        if self.ser:
            self._close()
        super().closeEvent(e)


STYLE = f"""
QWidget {{ background:{BG}; color:{TEXT}; font-family:'Malgun Gothic','Segoe UI'; font-size:13px; }}
QFrame#card {{ background:{PANEL}; border:1px solid {EDGE}; border-radius:10px; }}
QFrame#card QLabel, QFrame#card QCheckBox {{ background:transparent; }}
QLabel#title {{ font-size:18px; font-weight:bold; }}
QLabel#section {{ font-size:14px; font-weight:bold; }}
QLabel#key {{ color:{SUB}; }}
QLabel#val {{ font-size:15px; font-weight:bold; }}
QLabel#hint {{ color:{SUB}; font-size:12px; }}
QGroupBox {{ background:transparent; border:1px solid {EDGE}; border-radius:8px; margin-top:14px;
             padding:10px; font-weight:bold; }}
QGroupBox::title {{ subcontrol-origin:margin; left:10px; padding:0 4px; color:{SUB}; }}
QPushButton {{ background:#263241; border:1px solid {EDGE}; border-radius:6px; padding:6px 14px; }}
QPushButton:hover {{ background:#2f3e50; }}
QPushButton:pressed {{ background:#1d2733; }}
QPushButton:disabled {{ color:#5d6b7a; background:#1c242e; }}
QPushButton#primary {{ background:{ACCENT}; border:none; color:white; font-weight:bold; }}
QPushButton#floor {{ font-size:20px; font-weight:bold; }}
QPushButton#floor[lit="true"] {{ background:#3a2f12; border:2px solid #ffb020; color:#ffb020; }}
QPushButton#stop {{ background:#a82828; border:none; color:white; font-size:16px; font-weight:bold; }}
QPushButton#stop:hover {{ background:#c43030; }}
QPushButton#stop:disabled {{ background:#3a2424; color:#8a6a6a; }}
QComboBox, QLineEdit {{ background:#0f141a; border:1px solid {EDGE}; border-radius:6px; padding:5px 8px; }}
QPlainTextEdit {{ background:#0b0f14; border:1px solid {EDGE}; border-radius:6px;
                  font-family:Consolas,'D2Coding',monospace; font-size:12px; }}
QCheckBox::indicator {{ width:14px; height:14px; border:1px solid #6b7a8a; border-radius:3px; background:#0f141a; }}
QCheckBox::indicator:checked {{ background:{ACCENT}; border-color:{ACCENT}; }}
QSplitter::handle {{ background:{BG}; width:8px; }}
"""


def main():
    app = QApplication(sys.argv)
    app.setStyleSheet(STYLE)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
