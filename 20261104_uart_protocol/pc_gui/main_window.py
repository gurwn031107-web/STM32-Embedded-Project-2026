"""
GUI (PySide6 + pyqtgraph).

스레드 구조
  - 메인 스레드: 화면, 타이머(폴링/포트 스캔)
  - SerialWorker 스레드: 시리얼 송수신
  - 포트 확인(probe) 스레드: 자동 감지 때 잠깐 생성
  다른 스레드의 결과는 WorkerBridge 의 시그널로만 메인 스레드에 전달된다.
"""
from __future__ import annotations

import html
import threading
import time
from collections import deque
from datetime import datetime

import pyqtgraph as pg
from PySide6.QtCore import QObject, Qt, QTimer, Signal, Slot
from PySide6.QtGui import QCloseEvent, QFontDatabase
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QSplitter,
    QVBoxLayout,
    QWidget,
)

import config
import protocol as P
from port_scanner import SIM_DEVICE, PortInfo, find_board, list_ports
from serial_worker import SerialWorker
from simulator import SimulatedTransport, VirtualBoard

POLL_CMDS = frozenset({P.CMD_TEMP_HUMID_REQ, P.CMD_DISTANCE_REQ,
                       P.CMD_TEMP_HUMID_RSP, P.CMD_DISTANCE_RSP})

LOG_COLORS = {"TX": "#1d4ed8", "RX": "#047857", "INFO": "#4b5563",
              "WARN": "#b45309", "ERR": "#b91c1c"}

STYLE = """
QGroupBox { font-weight: 600; }
QPushButton#ledButton {
    font-size: 15px; font-weight: 700; min-height: 56px;
    border: 2px solid #9ca3af; border-radius: 10px;
    background: #f3f4f6; color: #374151;
}
QPushButton#ledButton[state="on"]      { background: #fde047; border-color: #ca8a04; color: #713f12; }
QPushButton#ledButton[state="pending"] { background: #e5e7eb; border-style: dashed; color: #6b7280; }
QPushButton#ledButton:disabled         { background: #f9fafb; border-color: #e5e7eb; color: #9ca3af; }
QFrame#valueCard { background: #ffffff; border: 1px solid #d1d5db; border-radius: 10px; }
QLabel#cardTitle { color: #6b7280; font-size: 13px; }
QLabel#cardValue { color: #111827; font-size: 30px; font-weight: 700; }
QLabel#cardSub   { color: #6b7280; font-size: 12px; }
QLabel#cardSub[error="true"] { color: #dc2626; }
"""


def _repolish(widget: QWidget) -> None:
    """동적 속성을 바꾼 뒤 스타일시트를 다시 적용."""
    widget.style().unpolish(widget)
    widget.style().polish(widget)


# ==================================================================== 스레드 -> GUI 다리
class WorkerBridge(QObject):
    """
    다른 스레드에서 emit 해도 메인 스레드의 슬롯에서 실행된다 (Qt 의 queued connection).
    gen: 연결마다 1씩 늘어나는 번호. 이전 연결에서 늦게 도착한 시그널을 걸러낸다.
    """
    tx = Signal(int, object, object)              # gen, raw, request
    rx = Signal(int, object, object)              # gen, raw, packet
    response = Signal(int, object, object)        # gen, request, response
    bad_response = Signal(int, object, object, str)
    timeout = Signal(int, object)
    unexpected = Signal(int, object)
    parse_error = Signal(int, object)
    disconnected = Signal(int, str)
    probe_done = Signal(object)                   # list[ProbeResult]


class _Listener:
    """SerialWorker 콜백 -> WorkerBridge 시그널. 연결마다 하나씩 만든다."""

    def __init__(self, bridge: WorkerBridge, gen: int) -> None:
        self.bridge = bridge
        self.gen = gen

    def on_tx(self, raw, request): self.bridge.tx.emit(self.gen, raw, request)
    def on_rx(self, raw, packet): self.bridge.rx.emit(self.gen, raw, packet)
    def on_response(self, request, response): self.bridge.response.emit(self.gen, request, response)
    def on_bad_response(self, request, packet, message):
        self.bridge.bad_response.emit(self.gen, request, packet, message)
    def on_timeout(self, request): self.bridge.timeout.emit(self.gen, request)
    def on_unexpected(self, packet): self.bridge.unexpected.emit(self.gen, packet)
    def on_parse_error(self, error): self.bridge.parse_error.emit(self.gen, error)
    def on_disconnected(self, reason): self.bridge.disconnected.emit(self.gen, reason)


# ==================================================================== 센서 값 카드
class ValueCard(QFrame):
    def __init__(self, title: str, unit: str) -> None:
        super().__init__()
        self.setObjectName("valueCard")
        self.unit = unit
        lay = QVBoxLayout(self)
        lay.setContentsMargins(14, 10, 14, 10)
        self.title_label = QLabel(title)
        self.title_label.setObjectName("cardTitle")
        self.value_label = QLabel("--")
        self.value_label.setObjectName("cardValue")
        self.sub_label = QLabel("측정 전")
        self.sub_label.setObjectName("cardSub")
        for w in (self.title_label, self.value_label, self.sub_label):
            w.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lay.addWidget(w)

    def set_value(self, value) -> None:
        self.value_label.setText(f"{value} {self.unit}")
        self._set_sub(f"갱신 {datetime.now():%H:%M:%S}", error=False)

    def set_error(self, message: str) -> None:
        """마지막 값은 그대로 두고 아래 줄에 오류만 표시."""
        self._set_sub(message, error=True)

    def set_note(self, message: str) -> None:
        self._set_sub(message, error=False)

    def _set_sub(self, text: str, error: bool) -> None:
        self.sub_label.setText(text)
        self.sub_label.setProperty("error", error)
        _repolish(self.sub_label)


# ==================================================================== 메인 창
class MainWindow(QMainWindow):
    def __init__(self, sim: bool = False) -> None:
        super().__init__()
        self.sim = sim
        self.setWindowTitle("Nucleo-F411 UART 제어기" + ("  [가상 보드]" if sim else ""))
        self.resize(1120, 800)

        # 연결 상태
        self.bridge = WorkerBridge()
        self.worker: SerialWorker | None = None
        self.gen = 0
        self.connected_port: str | None = None
        self.consecutive_timeouts = 0
        self.tx_count = self.rx_count = self.err_count = 0

        # 자동 감지 상태
        self.probing = False
        self.last_probe_set: frozenset | None = None
        self.last_probe_time = 0.0
        self.last_probe_summary = ""
        self._combo_snapshot: list | None = None

        # LED / 센서 상태
        self.led_states = [False] * P.LED_COUNT
        self.led_pending = [False] * P.LED_COUNT
        self.temp_hist: deque = deque()
        self.humid_hist: deque = deque()
        self.dist_hist: deque = deque()

        self._build_ui()
        self._connect_signals()

        self.scan_timer = QTimer(self)
        self.scan_timer.setInterval(config.PORT_SCAN_INTERVAL_MS)
        self.scan_timer.timeout.connect(self._scan_ports)
        self.scan_timer.start()

        self.th_timer = QTimer(self)
        self.th_timer.timeout.connect(self._poll_th)
        self.dist_timer = QTimer(self)
        self.dist_timer.timeout.connect(self._poll_dist)

        self._set_connected_ui(False)
        QTimer.singleShot(0, self._scan_ports)

    # ================================================================ 화면 구성
    def _build_ui(self) -> None:
        central = QWidget()
        root = QVBoxLayout(central)
        root.addWidget(self._build_connection_box())

        middle = QHBoxLayout()
        middle.addWidget(self._build_led_box(), 2)
        middle.addWidget(self._build_sensor_box(), 3)
        root.addLayout(middle)

        splitter = QSplitter(Qt.Orientation.Vertical)
        splitter.addWidget(self._build_graph_box())
        splitter.addWidget(self._build_log_box())
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        root.addWidget(splitter, 1)

        self.setCentralWidget(central)
        self.setStyleSheet(STYLE)

        self.counter_label = QLabel()
        self.statusBar().addPermanentWidget(self.counter_label)
        self._update_counters()

    def _build_connection_box(self) -> QGroupBox:
        box = QGroupBox("연결")
        lay = QHBoxLayout(box)
        lay.addWidget(QLabel("포트"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(360)
        lay.addWidget(self.port_combo)
        self.refresh_btn = QPushButton("새로고침")
        self.refresh_btn.setToolTip("포트 목록을 다시 읽고, 자동 연결이 켜져 있으면 보드를 바로 다시 찾습니다")
        lay.addWidget(self.refresh_btn)
        self.connect_btn = QPushButton("연결")
        self.connect_btn.setMinimumWidth(90)
        lay.addWidget(self.connect_btn)
        self.auto_cb = QCheckBox("자동 연결")
        self.auto_cb.setChecked(True)
        self.auto_cb.setToolTip("ST-LINK 포트 중 보드가 응답하는 포트에 자동으로 연결하고, "
                                "USB를 다시 꽂으면 다시 연결합니다")
        lay.addWidget(self.auto_cb)
        lay.addStretch(1)
        self.conn_status = QLabel()
        lay.addWidget(self.conn_status)
        return box

    def _build_led_box(self) -> QGroupBox:
        box = QGroupBox("LED")
        grid = QGridLayout(box)
        self.led_buttons: list[QPushButton] = []
        for i in range(P.LED_COUNT):
            b = QPushButton()
            b.setObjectName("ledButton")
            b.setCursor(Qt.CursorShape.PointingHandCursor)
            b.clicked.connect(lambda _checked=False, idx=i: self._on_led_clicked(idx))
            grid.addWidget(b, i // 2, i % 2)
            self.led_buttons.append(b)
        row = QHBoxLayout()
        self.all_on_btn = QPushButton("전체 ON")
        self.all_off_btn = QPushButton("전체 OFF")
        self.led_read_btn = QPushButton("상태 읽기")
        for b in (self.all_on_btn, self.all_off_btn, self.led_read_btn):
            row.addWidget(b)
        grid.addLayout(row, 2, 0, 1, 2)
        for i in range(P.LED_COUNT):
            self._render_led(i)
        return box

    def _build_sensor_box(self) -> QGroupBox:
        box = QGroupBox("센서")
        v = QVBoxLayout(box)
        cards = QHBoxLayout()
        self.temp_card = ValueCard("온도 (DHT11)", "°C")
        self.humid_card = ValueCard("습도 (DHT11)", "%")
        self.dist_card = ValueCard("거리 (초음파)", "cm")
        for c in (self.temp_card, self.humid_card, self.dist_card):
            cards.addWidget(c)
        v.addLayout(cards)

        ctrl = QGridLayout()
        self.th_auto_cb = QCheckBox("온습도 자동 측정")
        self.th_auto_cb.setChecked(True)
        self.th_spin = QSpinBox()
        self.th_spin.setRange(config.TH_POLL_MIN_MS, 60000)
        self.th_spin.setSingleStep(500)
        self.th_spin.setValue(config.TH_POLL_DEFAULT_MS)
        self.th_spin.setSuffix(" ms")
        self.th_spin.setToolTip(f"DHT11은 측정 간격이 최소 {config.TH_POLL_MIN_MS} ms 필요합니다")
        self.dist_auto_cb = QCheckBox("거리 자동 측정")
        self.dist_auto_cb.setChecked(True)
        self.dist_spin = QSpinBox()
        self.dist_spin.setRange(config.DIST_POLL_MIN_MS, 10000)
        self.dist_spin.setSingleStep(100)
        self.dist_spin.setValue(config.DIST_POLL_DEFAULT_MS)
        self.dist_spin.setSuffix(" ms")
        self.measure_btn = QPushButton("1회 측정")
        ctrl.addWidget(self.th_auto_cb, 0, 0)
        ctrl.addWidget(self.th_spin, 0, 1)
        ctrl.addWidget(self.dist_auto_cb, 1, 0)
        ctrl.addWidget(self.dist_spin, 1, 1)
        ctrl.addWidget(self.measure_btn, 0, 2, 2, 1)
        ctrl.setColumnStretch(3, 1)
        v.addLayout(ctrl)
        return box

    def _build_graph_box(self) -> QGroupBox:
        box = QGroupBox(f"실시간 그래프 (최근 {config.GRAPH_WINDOW_S}초)")
        lay = QHBoxLayout(box)
        pg.setConfigOptions(antialias=True, background="w", foreground="#374151")

        self.dist_plot = pg.PlotWidget()
        self.dist_plot.setTitle("초음파 거리")
        self.dist_plot.setLabel("left", "거리 (cm)")
        self.dist_plot.setLabel("bottom", "시간 (s)")
        self.dist_plot.showGrid(x=True, y=True, alpha=0.25)
        self.dist_plot.setXRange(-config.GRAPH_WINDOW_S, 0, padding=0)
        self.dist_plot.enableAutoRange(axis="y")
        self.dist_plot.setMouseEnabled(x=False, y=False)
        self.dist_curve = self.dist_plot.plot(pen=pg.mkPen("#2563eb", width=2))

        self.th_plot = pg.PlotWidget()
        self.th_plot.setTitle("온도 / 습도")
        self.th_plot.setLabel("bottom", "시간 (s)")
        self.th_plot.showGrid(x=True, y=True, alpha=0.25)
        self.th_plot.setXRange(-config.GRAPH_WINDOW_S, 0, padding=0)
        self.th_plot.setYRange(0, 100, padding=0)
        self.th_plot.setMouseEnabled(x=False, y=False)
        self.th_plot.addLegend(offset=(10, 10))
        self.temp_curve = self.th_plot.plot(
            pen=pg.mkPen("#dc2626", width=2), name="온도 (°C)",
            symbol="o", symbolSize=5, symbolBrush="#dc2626", symbolPen=None)
        self.humid_curve = self.th_plot.plot(
            pen=pg.mkPen("#0891b2", width=2), name="습도 (%)",
            symbol="o", symbolSize=5, symbolBrush="#0891b2", symbolPen=None)

        lay.addWidget(self.dist_plot)
        lay.addWidget(self.th_plot)
        return box

    def _build_log_box(self) -> QGroupBox:
        box = QGroupBox("통신 로그")
        v = QVBoxLayout(box)
        top = QHBoxLayout()
        self.show_poll_cb = QCheckBox("센서 폴링 패킷도 표시")
        self.show_poll_cb.setChecked(True)
        self.show_poll_cb.setToolTip("끄면 정상적인 센서 요청/응답은 숨기고 LED 명령과 오류만 보여줍니다")
        self.clear_log_btn = QPushButton("지우기")
        top.addWidget(self.show_poll_cb)
        top.addStretch(1)
        top.addWidget(self.clear_log_btn)
        v.addLayout(top)
        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setMaximumBlockCount(config.LOG_MAX_LINES)
        self.log_view.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self.log_view.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        v.addWidget(self.log_view)
        return box

    def _connect_signals(self) -> None:
        b = self.bridge
        b.tx.connect(self._on_tx)
        b.rx.connect(self._on_rx)
        b.response.connect(self._on_response)
        b.bad_response.connect(self._on_bad_response)
        b.timeout.connect(self._on_timeout)
        b.unexpected.connect(self._on_unexpected)
        b.parse_error.connect(self._on_parse_error)
        b.disconnected.connect(self._on_disconnected)
        b.probe_done.connect(self._on_probe_done)

        self.connect_btn.clicked.connect(self._on_connect_clicked)
        self.refresh_btn.clicked.connect(self._on_refresh_clicked)
        self.auto_cb.toggled.connect(self._on_auto_toggled)
        self.all_on_btn.clicked.connect(lambda: self._set_all_leds(True))
        self.all_off_btn.clicked.connect(lambda: self._set_all_leds(False))
        self.led_read_btn.clicked.connect(self._request_led_status)
        self.measure_btn.clicked.connect(self._measure_once)
        self.th_auto_cb.toggled.connect(self._on_poll_setting_changed)
        self.dist_auto_cb.toggled.connect(self._on_poll_setting_changed)
        self.th_spin.valueChanged.connect(self._on_poll_setting_changed)
        self.dist_spin.valueChanged.connect(self._on_poll_setting_changed)
        self.clear_log_btn.clicked.connect(self.log_view.clear)

    # ================================================================ 포트 감지 / 연결
    @Slot()
    def _scan_ports(self) -> None:
        if self.sim:
            ports = [PortInfo(SIM_DEVICE, "가상 보드 (테스트용)", True)]
        else:
            try:
                ports = list_ports()
            except Exception as e:  # noqa: BLE001 - 목록 읽기 실패가 프로그램을 멈추지 않도록
                self._log("ERR", f"포트 목록 읽기 실패: {e}")
                return
        self._update_port_combo(ports)

        if self.worker is not None:
            if self.connected_port not in {p.device for p in ports}:
                self._disconnect(reason="USB 분리 감지 (포트가 사라짐)")
            return

        if not self.auto_cb.isChecked() or self.probing:
            self._refresh_conn_status()
            return
        candidates = [p.device for p in ports if p.is_stm]
        if not candidates:
            self._refresh_conn_status()
            return
        if self.sim:
            self._connect(SIM_DEVICE, manual=False)
            return
        cand_set = frozenset(candidates)
        if (cand_set == self.last_probe_set
                and time.monotonic() - self.last_probe_time < config.PROBE_RETRY_S):
            return
        self._start_probe(candidates)

    def _start_probe(self, candidates: list[str]) -> None:
        self.probing = True
        self.last_probe_set = frozenset(candidates)
        self.last_probe_time = time.monotonic()
        self._refresh_conn_status()
        bridge = self.bridge

        def work() -> None:
            bridge.probe_done.emit(find_board(candidates))

        threading.Thread(target=work, name="port-probe", daemon=True).start()

    @Slot(object)
    def _on_probe_done(self, results) -> None:
        self.probing = False
        found = next((r for r in results if r.found), None)
        if found is None:
            summary = ", ".join(f"{r.device}: {r.message}" for r in results)
            if summary != self.last_probe_summary:
                self._log("WARN", f"보드를 찾지 못함 — {summary}")
                self.last_probe_summary = summary
            self._refresh_conn_status()
            return
        self.last_probe_summary = ""
        if self.worker is None and self.auto_cb.isChecked():
            self._log("INFO", f"{found.device} 에서 보드 응답 확인 (자동 감지)")
            self._connect(found.device, manual=False)
        else:
            self._refresh_conn_status()

    def _update_port_combo(self, ports: list[PortInfo]) -> None:
        snapshot = [(p.device, p.description, p.is_stm) for p in ports]
        if snapshot == self._combo_snapshot:
            return
        self._combo_snapshot = snapshot
        current = self.connected_port or self.port_combo.currentData()
        self.port_combo.blockSignals(True)
        self.port_combo.clear()
        for p in ports:
            self.port_combo.addItem(p.label, p.device)
        if not ports:
            self.port_combo.addItem("(시리얼 포트 없음)", None)
        idx = self.port_combo.findData(current) if current else -1
        if idx >= 0:
            self.port_combo.setCurrentIndex(idx)
        self.port_combo.blockSignals(False)

    def _connect(self, device: str, manual: bool) -> bool:
        if self.worker is not None:
            return False
        self.gen += 1
        worker = SerialWorker(_Listener(self.bridge, self.gen))
        try:
            if device == SIM_DEVICE:
                worker.open_transport(SimulatedTransport(VirtualBoard()), SIM_DEVICE)
            else:
                worker.open_port(device)
        except (OSError, ValueError) as e:      # serial.SerialException 은 OSError 의 하위 클래스
            msg = f"{device} 포트를 열 수 없습니다: {e}"
            self._log("ERR", msg)
            if manual:
                QMessageBox.warning(
                    self, "연결 실패",
                    msg + "\n\nTera Term, CubeIDE 시리얼 터미널 등 다른 프로그램이 "
                          "이 포트를 사용 중인지 확인하세요.")
            return False

        self.worker = worker
        self.connected_port = device
        self.consecutive_timeouts = 0
        self.led_pending = [False] * P.LED_COUNT
        idx = self.port_combo.findData(device)
        if idx >= 0:
            self.port_combo.setCurrentIndex(idx)
        how = "수동" if manual else "자동"
        self._log("INFO", f"{device} 연결됨 ({config.BAUDRATE} bps, 8N1, {how})")
        self.statusBar().showMessage(f"{device} 연결됨", 3000)
        self._set_connected_ui(True)
        for card in (self.temp_card, self.humid_card, self.dist_card):
            card.set_note("측정 대기")
        self._request_led_status()               # 실제 LED 상태로 버튼 맞추기
        self._apply_poll_timers(poll_now=True)
        return True

    def _disconnect(self, reason: str | None = None, manual: bool = False) -> None:
        worker, self.worker = self.worker, None
        if worker is None:
            return
        self.gen += 1                            # 이전 연결에서 늦게 오는 시그널 무시
        worker.close()
        port, self.connected_port = self.connected_port, None
        self.th_timer.stop()
        self.dist_timer.stop()
        self.led_pending = [False] * P.LED_COUNT
        self.last_probe_set = None               # 다시 꽂으면 바로 감지하도록
        self._set_connected_ui(False)
        if manual:
            self._log("INFO", f"{port} 연결 해제")
        else:
            self._log("ERR", f"{port} 연결 끊김: {reason}")
            self.statusBar().showMessage(f"연결 끊김: {reason}", 5000)
        for card in (self.temp_card, self.humid_card, self.dist_card):
            card.set_note("연결 안 됨")

    @Slot()
    def _on_connect_clicked(self) -> None:
        if self.worker is not None:
            # 수동으로 끊었을 때는 곧바로 자동 재연결되지 않도록 자동 연결을 끈다
            self.auto_cb.setChecked(False)
            self._disconnect(manual=True)
            return
        device = self.port_combo.currentData()
        if not device:
            QMessageBox.information(self, "포트 선택", "연결할 포트를 선택하세요.")
            return
        self._connect(device, manual=True)

    @Slot()
    def _on_refresh_clicked(self) -> None:
        self._combo_snapshot = None
        self.last_probe_set = None
        self._scan_ports()

    @Slot(bool)
    def _on_auto_toggled(self, checked: bool) -> None:
        if checked and self.worker is None:
            self.last_probe_set = None
            self._scan_ports()
        self._refresh_conn_status()

    # ================================================================ LED
    def _on_led_clicked(self, i: int) -> None:
        if self.worker is None or self.led_pending[i]:
            return
        self._send_led(i, not self.led_states[i])

    def _set_all_leds(self, on: bool) -> None:
        if self.worker is None:
            return
        for i in range(P.LED_COUNT):
            if not self.led_pending[i]:
                self._send_led(i, on)

    def _send_led(self, i: int, on: bool) -> None:
        if self.worker.submit(P.led_set(i, on)):
            self.led_pending[i] = True
            self._render_led(i)
        else:
            self.statusBar().showMessage("명령 대기열이 가득 찼습니다. 잠시 후 다시 시도하세요.", 3000)

    @Slot()
    def _request_led_status(self) -> None:
        if self.worker is not None:
            self.worker.submit(P.led_status_req(), coalesce=True)

    def _render_led(self, i: int) -> None:
        b = self.led_buttons[i]
        if self.led_pending[i]:
            state, text = "pending", f"LED{i}\n…"
        elif self.led_states[i]:
            state, text = "on", f"LED{i}\nON"
        else:
            state, text = "off", f"LED{i}\nOFF"
        b.setText(text)
        b.setProperty("state", state)
        _repolish(b)

    # ================================================================ 센서 폴링
    @Slot()
    def _poll_th(self) -> None:
        if self.worker is not None:
            self.worker.submit(P.temp_humid_req(), coalesce=True)

    @Slot()
    def _poll_dist(self) -> None:
        if self.worker is not None:
            self.worker.submit(P.distance_req(), coalesce=True)

    @Slot()
    def _measure_once(self) -> None:
        self._poll_th()
        self._poll_dist()

    def _on_poll_setting_changed(self, *_args) -> None:
        # 주기만 바꿀 때는 즉시 요청하지 않는다 (DHT11 최소 측정 간격 보호)
        self._apply_poll_timers(poll_now=False)

    def _apply_poll_timers(self, poll_now: bool = False) -> None:
        connected = self.worker is not None
        for timer, cb, spin, poll in (
            (self.th_timer, self.th_auto_cb, self.th_spin, self._poll_th),
            (self.dist_timer, self.dist_auto_cb, self.dist_spin, self._poll_dist),
        ):
            if connected and cb.isChecked():
                if timer.interval() != spin.value() or not timer.isActive():
                    timer.start(spin.value())
                    if poll_now:
                        poll()
            else:
                timer.stop()

    # ================================================================ 워커 시그널 처리
    def _should_log(self, cmd: int) -> bool:
        return self.show_poll_cb.isChecked() or cmd not in POLL_CMDS

    @Slot(int, object, object)
    def _on_tx(self, gen: int, raw: bytes, req: P.Request) -> None:
        if gen != self.gen:
            return
        self.tx_count += 1
        if self._should_log(req.cmd):
            self._log("TX", P.describe_request(req), raw)
        self._update_counters()

    @Slot(int, object, object)
    def _on_rx(self, gen: int, raw: bytes, packet: P.Packet) -> None:
        if gen != self.gen:
            return
        self.rx_count += 1
        if self._should_log(packet.cmd):
            self._log("RX", P.describe_packet(packet), raw)
        self._update_counters()

    @Slot(int, object, object)
    def _on_response(self, gen: int, req: P.Request, resp) -> None:
        if gen != self.gen:
            return
        if self.consecutive_timeouts:
            self.consecutive_timeouts = 0
            self._refresh_conn_status()

        if isinstance(resp, P.LedResult):
            i = req.led_index
            self.led_pending[i] = False
            if resp.ok:
                self.led_states[i] = bool(req.led_on)
            else:
                self._warn(f"LED{i} 제어 실패 (보드가 RSP_ERR 응답)")
            self._render_led(i)

        elif isinstance(resp, P.LedStatus):
            if resp.ok:
                self.led_states = list(resp.states)
                for i in range(P.LED_COUNT):
                    self._render_led(i)
            else:
                self._warn("LED 상태 읽기 실패 (보드가 RSP_ERR 응답)")

        elif isinstance(resp, P.TempHumid):
            if resp.ok:
                self.temp_card.set_value(f"{resp.temp_c:.1f}")
                self.humid_card.set_value(f"{resp.humid_pct:.1f}")
                self._push(self.temp_hist, resp.temp_c)
                self._push(self.humid_hist, resp.humid_pct)
                self._redraw_graphs()
            else:
                self.temp_card.set_error("센서 읽기 실패")
                self.humid_card.set_error("센서 읽기 실패")
                self._warn("온습도 센서 읽기 실패 (보드가 RSP_ERR 응답)")

        elif isinstance(resp, P.Distance):
            if resp.ok:
                self.dist_card.set_value(resp.cm)
                self._push(self.dist_hist, resp.cm)
                self._redraw_graphs()
            else:
                self.dist_card.set_error("측정 실패")
                self._warn("거리 측정 실패 (보드가 RSP_ERR 응답)")

    @Slot(int, object, object, str)
    def _on_bad_response(self, gen: int, req: P.Request, packet: P.Packet, message: str) -> None:
        if gen != self.gen:
            return
        self.err_count += 1
        self._log("ERR", f"응답 형식 오류: {message}")
        self._mark_request_failed(req, "응답 형식 오류")
        self._update_counters()

    @Slot(int, object)
    def _on_timeout(self, gen: int, req: P.Request) -> None:
        if gen != self.gen:
            return
        self.err_count += 1
        self.consecutive_timeouts += 1
        ms = int(config.RESPONSE_TIMEOUT_S * 1000)
        self._log("ERR", f"{P.describe_request(req)} 응답 없음 ({ms} ms 타임아웃)")
        self._mark_request_failed(req, "응답 없음")
        self._refresh_conn_status()
        self._update_counters()

    @Slot(int, object)
    def _on_unexpected(self, gen: int, packet: P.Packet) -> None:
        if gen != self.gen:
            return
        self._log("WARN", f"기다리지 않던 패킷 무시 (늦게 온 응답?): {P.cmd_name(packet.cmd)}")

    @Slot(int, object)
    def _on_parse_error(self, gen: int, error) -> None:
        if gen != self.gen:
            return
        self.err_count += 1
        self._log("ERR", error.message, error.raw)
        self._update_counters()

    @Slot(int, str)
    def _on_disconnected(self, gen: int, reason: str) -> None:
        if gen != self.gen or self.worker is None:
            return
        self._disconnect(reason=reason)

    def _mark_request_failed(self, req: P.Request, message: str) -> None:
        if req.led_index is not None:
            self.led_pending[req.led_index] = False
            self._render_led(req.led_index)
            self._warn(f"LED{req.led_index} 제어 실패 ({message})")
        elif req.cmd == P.CMD_TEMP_HUMID_REQ:
            self.temp_card.set_error(message)
            self.humid_card.set_error(message)
        elif req.cmd == P.CMD_DISTANCE_REQ:
            self.dist_card.set_error(message)

    # ================================================================ 그래프
    @staticmethod
    def _push(hist: deque, value) -> None:
        now = time.monotonic()
        hist.append((now, value))
        while hist and now - hist[0][0] > config.GRAPH_WINDOW_S:
            hist.popleft()

    def _redraw_graphs(self) -> None:
        now = time.monotonic()
        for curve, hist in ((self.dist_curve, self.dist_hist),
                            (self.temp_curve, self.temp_hist),
                            (self.humid_curve, self.humid_hist)):
            curve.setData([t - now for t, _ in hist], [v for _, v in hist])

    # ================================================================ 상태 표시 / 로그
    def _set_connected_ui(self, connected: bool) -> None:
        self.connect_btn.setText("연결 해제" if connected else "연결")
        self.port_combo.setEnabled(not connected)
        self.refresh_btn.setEnabled(not connected)
        for w in (*self.led_buttons, self.all_on_btn, self.all_off_btn,
                  self.led_read_btn, self.measure_btn):
            w.setEnabled(connected)
        for i in range(P.LED_COUNT):
            self._render_led(i)
        self._refresh_conn_status()

    def _refresh_conn_status(self) -> None:
        if self.worker is not None:
            if self.consecutive_timeouts >= config.NO_RESPONSE_WARN_COUNT:
                text, color = f"● {self.connected_port} 연결됨 — 보드 응답 없음", "#dc2626"
            else:
                text, color = f"● {self.connected_port} 연결됨", "#16a34a"
        elif self.probing:
            text, color = "● 보드 찾는 중…", "#d97706"
        elif self.auto_cb.isChecked():
            text, color = "● 연결 안 됨 (보드를 기다리는 중)", "#6b7280"
        else:
            text, color = "● 연결 안 됨", "#6b7280"
        self.conn_status.setText(text)
        self.conn_status.setStyleSheet(f"color: {color}; font-weight: 600;")

    def _update_counters(self) -> None:
        self.counter_label.setText(f"TX {self.tx_count}  ·  RX {self.rx_count}  ·  오류 {self.err_count}")

    def _warn(self, message: str) -> None:
        self.statusBar().showMessage(message, 5000)

    def _log(self, kind: str, text: str, raw: bytes | None = None) -> None:
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        hexs = P.hexdump(raw) if raw is not None else ""
        line = f"[{ts}] {kind:<4} {hexs:<21} {text}"
        color = LOG_COLORS.get(kind, "#111827")
        self.log_view.appendHtml(
            f'<span style="color:{color}">{html.escape(line).replace(" ", "&nbsp;")}</span>')

    # ================================================================ 종료
    def closeEvent(self, event: QCloseEvent) -> None:
        self.scan_timer.stop()
        self.th_timer.stop()
        self.dist_timer.stop()
        if self.worker is not None:
            self.worker.close()
            self.worker = None
        event.accept()
