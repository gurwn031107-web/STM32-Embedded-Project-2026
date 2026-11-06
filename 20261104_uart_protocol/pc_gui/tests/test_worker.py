"""SerialWorker + 가상 보드: 명령 큐, 응답 매칭, 타임아웃, USB 분리."""
import threading
import time

import protocol as P
from serial_worker import SerialWorker, WorkerListener
from simulator import SimulatedTransport, VirtualBoard


class Recorder(WorkerListener):
    def __init__(self):
        self.events = []
        self.cv = threading.Condition()

    def _add(self, *ev):
        with self.cv:
            self.events.append(ev)
            self.cv.notify_all()

    def on_tx(self, raw, request): self._add("tx", raw, request)
    def on_rx(self, raw, packet): self._add("rx", raw, packet)
    def on_response(self, request, response): self._add("response", request, response)
    def on_bad_response(self, request, packet, message): self._add("bad", request, packet, message)
    def on_timeout(self, request): self._add("timeout", request)
    def on_unexpected(self, packet): self._add("unexpected", packet)
    def on_parse_error(self, error): self._add("parse_error", error)
    def on_disconnected(self, reason): self._add("disconnected", reason)

    def of(self, kind):
        with self.cv:
            return [e for e in self.events if e[0] == kind]

    def wait_for(self, kind, count=1, timeout=3.0):
        end = time.monotonic() + timeout
        with self.cv:
            while len([e for e in self.events if e[0] == kind]) < count:
                left = end - time.monotonic()
                assert left > 0, f"'{kind}' 이벤트 {count}개를 기다리다 시간 초과: {self.events}"
                self.cv.wait(left)
        return self.of(kind)


def start(board=None):
    board = board or VirtualBoard(seed=1)
    tr = SimulatedTransport(board)
    rec = Recorder()
    w = SerialWorker(rec)
    w.open_transport(tr, "SIM")
    return w, tr, board, rec


def test_led_set_and_status_roundtrip():
    w, tr, board, rec = start()
    try:
        w.submit(P.led_set(1, True))
        w.submit(P.led_set(3, True))
        w.submit(P.led_status_req())
        res = rec.wait_for("response", 3)
        assert [r[1].cmd for r in res] == [0x11, 0x13, 0x14]       # 보낸 순서대로
        assert res[0][2] == P.LedResult(True)
        assert res[2][2].states == (False, True, False, True)
        assert board.leds == [False, True, False, True]
        assert [e[2].cmd for e in rec.of("rx")] == [0x91, 0x93, 0x94]  # 펌웨어처럼 CMD+0x80
        # 요청을 하나씩 보내므로 TX 와 RX 가 번갈아 나온다
        kinds = [e[0] for e in rec.events if e[0] in ("tx", "rx")]
        assert kinds == ["tx", "rx"] * 3
    finally:
        w.close()


def test_sensors():
    w, tr, board, rec = start()
    try:
        w.submit(P.temp_humid_req())
        w.submit(P.distance_req())
        res = rec.wait_for("response", 2)
        th, dist = res[0][2], res[1][2]
        assert th.ok and 0 <= th.temp_c <= 50 and 20 <= th.humid_pct <= 90
        assert dist.ok and 2 <= dist.cm <= 400
    finally:
        w.close()


def test_sensor_error_is_reported_as_not_ok():
    board = VirtualBoard(seed=1, sensor_error_rate=1.0)
    w, tr, board, rec = start(board)
    try:
        w.submit(P.temp_humid_req())
        assert rec.wait_for("response")[0][2] == P.TempHumid(False)
    finally:
        w.close()


def test_coalesce_keeps_one_queued_poll_per_cmd():
    w, tr, board, rec = start()
    try:
        board.muted = True                       # 응답이 없으니 큐에 쌓인다
        assert w.submit(P.distance_req(), coalesce=True)
        rec.wait_for("tx")                       # 첫 요청은 전송되어 응답 대기 중
        assert w.submit(P.distance_req(), coalesce=True)
        assert not w.submit(P.distance_req(), coalesce=True)   # 이미 대기 중이라 무시
        assert w.submit(P.led_set(0, True))      # LED 요청은 합치지 않음
        assert w.queued_count() == 2
    finally:
        w.close()


def test_timeout_then_next_request_still_works():
    w, tr, board, rec = start()
    try:
        board.muted = True
        t0 = time.monotonic()
        w.submit(P.temp_humid_req())
        rec.wait_for("timeout")
        assert time.monotonic() - t0 >= 0.25     # RESPONSE_TIMEOUT_S = 0.3
        board.muted = False
        w.submit(P.led_set(2, True))
        assert rec.wait_for("response")[0][2] == P.LedResult(True)
    finally:
        w.close()


def test_late_response_after_timeout_is_not_matched_to_next_request():
    w, tr, board, rec = start()
    try:
        board.muted = True
        w.submit(P.led_set(0, True))
        rec.wait_for("timeout")
        board.muted = False
        tr.inject(P.build_packet(P.CMD_LED_RSP, b"\x00"))   # 늦게 도착한 0x90
        rec.wait_for("unexpected")
        w.submit(P.led_set(1, True))
        res = rec.wait_for("response")
        assert len(res) == 1 and res[0][1].led_index == 1
    finally:
        w.close()


def test_noise_and_bad_checksum_do_not_break_communication():
    w, tr, board, rec = start()
    try:
        tr.inject(b"\x00\xFF" + bytes.fromhex("AA 05 A0 15 01 30 00 00"))   # 노이즈 + 체크섬 틀린 패킷
        errs = rec.wait_for("parse_error", 2)
        assert {e[1].kind for e in errs} == {"garbage", "checksum"}
        w.submit(P.distance_req())
        assert rec.wait_for("response")[0][2].ok
    finally:
        w.close()


def test_format_mismatch_reported_as_bad_response():
    class WrongFirmware(VirtualBoard):
        def handle(self, packet):
            if packet.cmd == P.CMD_DISTANCE_REQ:
                return P.Packet(P.CMD_DISTANCE_RSP, bytes([0, 0x84, 0x00]))   # 예전 3바이트 형식
            return super().handle(packet)

    w, tr, board, rec = start(WrongFirmware(seed=1))
    try:
        w.submit(P.distance_req())
        bad = rec.wait_for("bad")[0]
        assert "길이 불일치" in bad[3]
    finally:
        w.close()


def test_usb_unplug_reports_disconnect():
    w, tr, board, rec = start()
    tr.unplugged = True
    reason = rec.wait_for("disconnected")[0][1]
    assert "통신 오류" in reason
    assert not w.is_open
    assert not w.submit(P.led_status_req())
    w.close()


def test_manual_close_does_not_report_disconnect():
    w, tr, board, rec = start()
    w.close()
    time.sleep(0.1)
    assert rec.of("disconnected") == []
