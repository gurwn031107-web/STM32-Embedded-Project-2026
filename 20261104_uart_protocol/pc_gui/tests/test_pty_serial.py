"""
실제 pyserial 경로 검증 (리눅스/맥 전용, 윈도우에서는 자동으로 건너뜀).
가상 터미널(pty) 한쪽에 가상 보드를 붙이고, 다른 쪽을 pyserial 로 열어
포트 감지 핸드셰이크, 송수신, USB 분리 감지를 확인한다.
"""
import os
import select
import threading

import pytest

serial = pytest.importorskip("serial")
if not hasattr(os, "openpty"):
    pytest.skip("pty 를 지원하지 않는 OS", allow_module_level=True)

import protocol as P                               # noqa: E402
from port_scanner import probe_port               # noqa: E402
from serial_worker import SerialWorker            # noqa: E402
from simulator import VirtualBoard                # noqa: E402
from test_worker import Recorder                  # noqa: E402


class PtyBoard:
    """pty 의 master 쪽에서 가상 보드 펌웨어를 돌린다. slave 경로가 'COM 포트' 역할."""

    def __init__(self, board):
        self.board = board
        self.master, self.slave = os.openpty()
        self.path = os.ttyname(self.slave)
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while not self._stop.is_set():
            r, _, _ = select.select([self.master], [], [], 0.05)
            if not r:
                continue
            try:
                data = os.read(self.master, 1024)
            except OSError:
                break
            rsp = self.board.receive(data)
            if rsp:
                os.write(self.master, rsp)

    def unplug(self):
        """USB 를 뽑은 것처럼 master 를 닫는다."""
        self._stop.set()
        self._thread.join(1)
        os.close(self.master)
        self.master = None

    def close(self):
        if self.master is not None:
            self.unplug()
        os.close(self.slave)


@pytest.fixture
def pty_board():
    pb = PtyBoard(VirtualBoard(seed=3))
    yield pb
    pb.close()


def test_probe_finds_board(pty_board):
    r = probe_port(pty_board.path)
    assert r.found, r.message


def test_probe_rejects_silent_device(pty_board):
    pty_board.board.muted = True
    r = probe_port(pty_board.path)
    assert not r.found and r.message == "응답 없음"


def test_probe_reports_missing_port():
    r = probe_port("/dev/does-not-exist")
    assert not r.found and r.message.startswith("열기 실패")


def test_worker_over_real_pyserial(pty_board):
    rec = Recorder()
    w = SerialWorker(rec)
    w.open_port(pty_board.path)
    try:
        for i in range(4):
            w.submit(P.led_set(i, True))
        w.submit(P.led_status_req())
        w.submit(P.temp_humid_req())
        w.submit(P.distance_req())
        res = rec.wait_for("response", 7)
        assert all(r[2].ok for r in res)
        assert res[4][2].states == (True, True, True, True)
        assert rec.of("timeout") == [] and rec.of("parse_error") == []
    finally:
        w.close()


def test_worker_detects_unplug(pty_board):
    rec = Recorder()
    w = SerialWorker(rec)
    w.open_port(pty_board.path)
    w.submit(P.led_status_req())
    rec.wait_for("response")
    pty_board.unplug()
    reason = rec.wait_for("disconnected")[0][1]
    assert "통신 오류" in reason
    w.close()
