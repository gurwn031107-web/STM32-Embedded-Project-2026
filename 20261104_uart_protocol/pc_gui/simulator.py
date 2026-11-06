"""
가상 보드 (보드 없이 개발/테스트용).

  python main.py --sim   ->  실제 Nucleo 펌웨어처럼 응답하는 가상 보드로 GUI 실행

VirtualBoard        : 요청 패킷을 받아 응답 패킷을 만드는 '펌웨어' 역할
                      (20261104_uart_protocol 펌웨어의 packet_do_func 와 같은 응답 형식)
SimulatedTransport  : pyserial Serial 과 같은 모양(read / write / in_waiting / close)의 가상 포트
"""
from __future__ import annotations

import math
import random
import threading
import time
from collections import deque

import config
from packet_parser import PacketParser
from protocol import (
    CMD_DISTANCE_REQ,
    CMD_DISTANCE_RSP,
    CMD_LED0_SET,
    CMD_LED_STATUS_REQ,
    CMD_LED_STATUS_RSP,
    CMD_TEMP_HUMID_REQ,
    CMD_TEMP_HUMID_RSP,
    LED_COUNT,
    LED_SET_CMDS,
    RSP_ERR,
    RSP_OK,
    Packet,
)

DHT11_INTERVAL_S = 2.0


class VirtualBoard:
    def __init__(self, seed: int | None = None, sensor_error_rate: float = 0.0) -> None:
        self.leds = [False] * LED_COUNT
        self.sensor_error_rate = sensor_error_rate
        self.muted = False                 # True 면 아무 응답도 하지 않음 (타임아웃 확인용)
        self._rng = random.Random(seed)
        self._t0 = time.monotonic()
        self._temp = 24.0
        self._humid = 45.0
        self._th_cache = (24, 0, 45, 0)
        self._th_time = -1e9
        self._parser = PacketParser()

    def receive(self, data: bytes) -> bytes:
        """PC 가 보낸 바이트를 받아, 보드가 돌려보낼 바이트를 반환한다."""
        out = bytearray()
        for ev in self._parser.feed(data):
            if isinstance(ev, Packet):
                rsp = self.handle(ev)
                if rsp is not None:
                    out += rsp.to_bytes()
        return bytes(out)

    def handle(self, packet: Packet) -> Packet | None:
        if self.muted:
            return None
        cmd, d = packet.cmd, packet.data

        if cmd in LED_SET_CMDS:            # 응답 CMD = 요청 CMD + 0x80 (0x90~0x93)
            if len(d) < 1:
                return Packet(cmd + 0x80, bytes([RSP_ERR]))
            self.leds[cmd - CMD_LED0_SET] = d[0] == 1
            return Packet(cmd + 0x80, bytes([RSP_OK]))

        if cmd == CMD_LED_STATUS_REQ:      # [LED0, LED1, LED2, LED3] 각 0/1
            return Packet(CMD_LED_STATUS_RSP, bytes(1 if on else 0 for on in self.leds))

        if cmd == CMD_TEMP_HUMID_REQ:      # [온도 정수, 온도 소수, 습도 정수, 습도 소수]
            if self._sensor_fails():
                return Packet(CMD_TEMP_HUMID_RSP, bytes([RSP_ERR]))
            return Packet(CMD_TEMP_HUMID_RSP, bytes(self._read_dht11()))

        if cmd == CMD_DISTANCE_REQ:        # [상위바이트, 하위바이트] cm
            if self._sensor_fails():
                return Packet(CMD_DISTANCE_RSP, bytes([RSP_ERR]))
            cm = self._read_distance()
            return Packet(CMD_DISTANCE_RSP, bytes([cm >> 8, cm & 0xFF]))

        return None                        # 모르는 명령은 무시

    # ------------------------------------------------------------ 가짜 센서
    def _sensor_fails(self) -> bool:
        return self._rng.random() < self.sensor_error_rate

    def _read_dht11(self) -> tuple[int, int, int, int]:
        """DHT11 처럼 2초에 한 번만 새로 측정하고, 그 사이에는 저장된 값을 돌려준다."""
        now = time.monotonic()
        if now - self._th_time >= DHT11_INTERVAL_S:
            self._temp = min(50.0, max(0.0, self._temp + self._rng.uniform(-0.4, 0.4)))
            self._humid = min(90.0, max(20.0, self._humid + self._rng.uniform(-1.5, 1.5)))
            t10 = round(self._temp * 10)
            self._th_cache = (t10 // 10, t10 % 10, round(self._humid), 0)
            self._th_time = now
        return self._th_cache

    def _read_distance(self) -> int:
        """손을 천천히 앞뒤로 움직이는 것처럼 8초 주기로 변하는 거리 (cm)."""
        t = time.monotonic() - self._t0
        cm = 65 + 50 * math.sin(2 * math.pi * t / 8.0) + self._rng.uniform(-1.5, 1.5)
        return int(min(400, max(2, round(cm))))


class SimulatedTransport:
    """VirtualBoard 와 연결된 가상 시리얼 포트."""

    def __init__(self, board: VirtualBoard, latency_s: float = 0.005,
                 read_timeout: float = config.READ_POLL_S) -> None:
        self.board = board
        self.unplugged = False             # True 로 바꾸면 USB 를 뽑은 것처럼 OSError 발생
        self._latency = latency_s
        self._timeout = read_timeout
        self._rx = bytearray()             # PC 가 읽어 갈 바이트
        self._scheduled: deque[tuple[float, bytes]] = deque()   # (도착 시각, 바이트)
        self._cond = threading.Condition()
        self._closed = False

    def _check(self) -> None:
        if self.unplugged:
            raise OSError("가상 USB 분리")
        if self._closed:
            raise OSError("포트가 닫혀 있음")

    def _deliver_due(self) -> None:        # self._cond 잠금 안에서 호출
        now = time.monotonic()
        while self._scheduled and self._scheduled[0][0] <= now:
            self._rx += self._scheduled.popleft()[1]

    @property
    def in_waiting(self) -> int:
        self._check()
        with self._cond:
            self._deliver_due()
            return len(self._rx)

    def read(self, size: int = 1) -> bytes:
        self._check()
        deadline = time.monotonic() + self._timeout
        with self._cond:
            while True:
                self._deliver_due()
                if self._rx or self._closed:
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                if self._scheduled:
                    remaining = min(remaining, max(0.0, self._scheduled[0][0] - time.monotonic()))
                self._cond.wait(remaining)
            out = bytes(self._rx[:size])
            del self._rx[:size]
            return out

    def write(self, data: bytes) -> int:
        self._check()
        rsp = self.board.receive(bytes(data))
        if rsp:
            self._schedule(rsp, self._latency)
        return len(data)

    def inject(self, data: bytes, delay_s: float = 0.0) -> None:
        """보드가 보낸 것처럼 임의의 바이트를 PC 쪽으로 흘려보낸다 (노이즈 테스트용)."""
        self._schedule(bytes(data), delay_s)

    def _schedule(self, data: bytes, delay_s: float) -> None:
        with self._cond:
            self._scheduled.append((time.monotonic() + delay_s, data))
            self._cond.notify_all()

    def reset_input_buffer(self) -> None:
        with self._cond:
            self._rx.clear()
            self._scheduled.clear()

    def close(self) -> None:
        with self._cond:
            self._closed = True
            self._cond.notify_all()
