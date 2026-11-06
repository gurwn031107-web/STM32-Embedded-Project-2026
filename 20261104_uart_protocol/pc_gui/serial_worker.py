"""
시리얼 송수신 담당 (별도 스레드).

  - 명령 큐: GUI 가 submit() 으로 넣은 요청을 하나씩 보낸다.
    응답을 받거나 타임아웃이 나야 다음 요청을 보낸다. (펌웨어가 수신 인터럽트 안에서
    응답을 보내므로, 응답 중에 다음 요청이 겹쳐 들어가지 않게 하는 효과도 있다.)
  - 수신: 바이트를 PacketParser 로 조립하고, 기다리던 응답이면 해석해서 알린다.
  - 결과는 listener 의 on_xxx() 콜백으로 알린다. 이 콜백은 수신 스레드에서 호출되므로
    GUI 쪽에서는 Qt 시그널로 바꿔 메인 스레드로 넘겨야 한다 (main_window.py 참고).

Qt 에 의존하지 않으므로 보드나 GUI 없이도 테스트할 수 있다.
"""
from __future__ import annotations

import collections
import threading
import time

import config
from packet_parser import PacketParser, ParseError
from protocol import Packet, ProtocolError, Request, Response, parse_response


class WorkerListener:
    """SerialWorker 가 호출하는 콜백 모음. 필요한 것만 재정의해서 쓴다."""

    def on_tx(self, raw: bytes, request: Request) -> None: ...
    def on_rx(self, raw: bytes, packet: Packet) -> None: ...
    def on_response(self, request: Request, response: Response) -> None: ...
    def on_bad_response(self, request: Request, packet: Packet, message: str) -> None: ...
    def on_timeout(self, request: Request) -> None: ...
    def on_unexpected(self, packet: Packet) -> None: ...
    def on_parse_error(self, error: ParseError) -> None: ...
    def on_disconnected(self, reason: str) -> None: ...


class SerialWorker:
    def __init__(self, listener: WorkerListener | None = None) -> None:
        self._listener = listener or WorkerListener()
        self._transport = None          # pyserial Serial 또는 SimulatedTransport
        self._name = ""
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._queue: collections.deque[Request] = collections.deque()
        self._parser = PacketParser()
        # 아래는 수신 스레드에서만 사용
        self._pending: Request | None = None
        self._deadline = 0.0
        self._quiet_until = 0.0

    # ------------------------------------------------------------ 열기/닫기
    @property
    def is_open(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    @property
    def port_name(self) -> str:
        return self._name

    def open_port(self, device: str) -> None:
        """실제 시리얼 포트를 연다. 실패하면 serial.SerialException(OSError) 발생."""
        import serial  # pyserial (가상 보드만 쓸 때는 없어도 되도록 여기서 import)

        ser = serial.Serial(
            device,
            config.BAUDRATE,
            timeout=config.READ_POLL_S,
            write_timeout=config.WRITE_TIMEOUT_S,
        )
        try:
            ser.reset_input_buffer()     # 연결 전에 쌓여 있던 바이트 버리기
        except OSError:
            pass
        self.open_transport(ser, device)

    def open_transport(self, transport, name: str) -> None:
        """이미 열린 포트 객체로 시작 (가상 보드, 테스트용)."""
        if self.is_open:
            raise RuntimeError("이미 열려 있습니다")
        self._transport = transport
        self._name = name
        self._stop.clear()
        self._parser.reset()
        with self._lock:
            self._queue.clear()
        self._pending = None
        self._quiet_until = 0.0
        self._thread = threading.Thread(target=self._run, name=f"serial-{name}", daemon=True)
        self._thread.start()

    def close(self, timeout: float = 1.0) -> None:
        self._stop.set()
        t = self._thread
        if t is not None and t is not threading.current_thread():
            t.join(timeout)
        self._thread = None
        self._close_transport()

    def _close_transport(self) -> None:
        with self._lock:
            tr, self._transport = self._transport, None
        if tr is not None:
            try:
                tr.close()
            except OSError:
                pass

    # ------------------------------------------------------------ 요청
    def submit(self, request: Request, coalesce: bool = False) -> bool:
        """
        요청을 대기열에 넣는다.
        coalesce=True 이면 같은 CMD 가 이미 대기 중일 때 넣지 않는다 (센서 폴링용).
        """
        if not self.is_open:
            return False
        with self._lock:
            if coalesce and any(r.cmd == request.cmd for r in self._queue):
                return False
            if len(self._queue) >= config.QUEUE_MAX:
                return False
            self._queue.append(request)
        return True

    def queued_count(self) -> int:
        with self._lock:
            return len(self._queue)

    # ------------------------------------------------------------ 스레드 본체
    def _run(self) -> None:
        reason = None
        try:
            while not self._stop.is_set():
                self._maybe_send(time.monotonic())
                data = self._read()
                now = time.monotonic()
                events = self._parser.feed(data, now) if data else self._parser.check_timeout(now)
                for ev in events:
                    self._handle_event(ev)
                if self._pending is not None and now >= self._deadline:
                    req, self._pending = self._pending, None
                    self._quiet_until = now + config.POST_TIMEOUT_QUIET_S
                    self._listener.on_timeout(req)
        except OSError as e:              # serial.SerialException 포함 (USB 분리 등)
            reason = f"통신 오류: {e}"
        except Exception as e:            # 예상 못 한 오류도 조용히 죽지 않도록 알림
            reason = f"내부 오류: {e!r}"
        finally:
            self._close_transport()
        if reason is not None and not self._stop.is_set():
            self._listener.on_disconnected(reason)

    def _read(self) -> bytes:
        tr = self._transport
        if tr is None:
            raise OSError("포트가 닫혔습니다")
        waiting = tr.in_waiting
        return tr.read(waiting if waiting > 0 else 1)

    def _maybe_send(self, now: float) -> None:
        if self._pending is not None or now < self._quiet_until:
            return
        with self._lock:
            if not self._queue:
                return
            req = self._queue.popleft()
        tr = self._transport
        if tr is None:
            raise OSError("포트가 닫혔습니다")
        raw = req.to_bytes()
        tr.write(raw)
        self._pending = req
        self._deadline = time.monotonic() + config.RESPONSE_TIMEOUT_S
        self._listener.on_tx(raw, req)

    def _handle_event(self, ev) -> None:
        if isinstance(ev, ParseError):
            self._listener.on_parse_error(ev)
            return
        packet: Packet = ev
        self._listener.on_rx(packet.to_bytes(), packet)
        req = self._pending
        if req is None or packet.cmd not in req.expects:
            self._listener.on_unexpected(packet)
            return
        self._pending = None
        try:
            resp = parse_response(packet)
        except ProtocolError as e:
            self._listener.on_bad_response(req, packet, str(e))
            return
        self._listener.on_response(req, resp)
