"""
포트 자동 감지.

  1) list_ports(): 연결된 COM 포트 목록. STMicro VID(0x0483)인 포트를 후보로 표시
  2) probe_port(): 후보 포트에 LED_STATUS_REQ 를 보내 0x94 응답이 오면 '우리 보드'로 확정
     (다른 ST 보드나 펌웨어가 다른 보드와 헷갈리지 않도록)
"""
from __future__ import annotations

import re
import time
from dataclasses import dataclass

import config
from packet_parser import PacketParser
from protocol import CMD_LED_STATUS_RSP, Packet, led_status_req

SIM_DEVICE = "SIM"


@dataclass(frozen=True)
class PortInfo:
    device: str          # 예: "COM5", "/dev/ttyACM0"
    description: str
    is_stm: bool         # ST-LINK 가상 COM 포트인지

    @property
    def label(self) -> str:
        tag = "  [ST-LINK]" if self.is_stm else ""
        if self.description and self.description != self.device:
            return f"{self.device} — {self.description}{tag}"
        return f"{self.device}{tag}"


@dataclass(frozen=True)
class ProbeResult:
    device: str
    found: bool
    message: str


def _natural_key(name: str):
    """COM10 이 COM9 뒤에 오도록 숫자를 숫자로 비교."""
    return [int(s) if s.isdigit() else s.lower() for s in re.split(r"(\d+)", name)]


def list_ports() -> list[PortInfo]:
    """현재 연결된 시리얼 포트 목록 (ST-LINK 포트가 앞에 옴)."""
    from serial.tools import list_ports as lp

    ports = []
    for p in lp.comports():
        if p.hwid == "n/a":          # 리눅스의 실제로 없는 /dev/ttyS* 등
            continue
        ports.append(PortInfo(p.device, p.description or "", p.vid == config.STM_VID))
    ports.sort(key=lambda x: (not x.is_stm, _natural_key(x.device)))
    return ports


def probe_port(device: str, timeout: float = config.HANDSHAKE_TIMEOUT_S) -> ProbeResult:
    """포트를 잠깐 열어 LED_STATUS_REQ 를 보내고 응답을 확인한다."""
    import serial

    try:
        with serial.Serial(device, config.BAUDRATE, timeout=config.READ_POLL_S,
                           write_timeout=config.WRITE_TIMEOUT_S) as ser:
            ser.reset_input_buffer()
            ser.write(led_status_req().to_bytes())
            parser = PacketParser()
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                waiting = ser.in_waiting
                data = ser.read(waiting if waiting > 0 else 1)
                if not data:
                    continue
                for ev in parser.feed(data):
                    if isinstance(ev, Packet) and ev.cmd == CMD_LED_STATUS_RSP:
                        return ProbeResult(device, True, "보드 응답 확인")
            return ProbeResult(device, False, "응답 없음")
    except (OSError, ValueError) as e:     # 다른 프로그램이 사용 중 등
        return ProbeResult(device, False, f"열기 실패 ({e})")


def find_board(candidates: list[str]) -> list[ProbeResult]:
    """후보 포트를 차례로 확인한다. 보드를 찾으면 거기서 멈춘다."""
    results = []
    for device in candidates:
        r = probe_port(device)
        results.append(r)
        if r.found:
            break
    return results
