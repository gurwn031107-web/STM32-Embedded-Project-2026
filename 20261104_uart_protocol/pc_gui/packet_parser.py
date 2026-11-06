"""
수신 바이트를 패킷으로 조립하는 파서.

    | SOF(0xAA) | LEN | CMD | PAYLOAD (LEN-1) | CHECKSUM |

ETX 가 없는 프로토콜이라 다음 두 가지를 신경 쓴다.
  1) 재동기화: DATA 안에 0xAA 가 있을 수 있으므로, 체크섬이 틀리면
     패킷 전체가 아니라 SOF 한 바이트만 버리고 그 다음부터 다시 0xAA 를 찾는다.
  2) 수신 타임아웃: 패킷 도중 바이트 간격이 INTER_BYTE_TIMEOUT_S 를 넘으면
     미완성 데이터를 정리한다. 이때도 남은 바이트 안의 정상 패킷은 살린다.
"""
from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Callable

import config
from protocol import MAX_LEN, SOF, Packet, checksum


@dataclass(frozen=True)
class ParseError:
    kind: str       # "checksum" | "garbage" | "timeout"
    raw: bytes
    message: str


ParseEvent = Packet | ParseError


class PacketParser:
    def __init__(
        self,
        inter_byte_timeout: float = config.INTER_BYTE_TIMEOUT_S,
        max_len: int = MAX_LEN,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self._timeout = inter_byte_timeout
        self._max_len = max_len
        self._clock = clock
        self._buf = bytearray()
        self._last_rx = 0.0

    def reset(self) -> None:
        self._buf.clear()

    @property
    def pending_bytes(self) -> int:
        return len(self._buf)

    def feed(self, data: bytes, now: float | None = None) -> list[ParseEvent]:
        """수신한 바이트를 넣고, 완성된 패킷/오류 목록을 돌려준다."""
        now = self._clock() if now is None else now
        events: list[ParseEvent] = []
        if self._buf and now - self._last_rx > self._timeout:
            events += self._scan(final=True)
        if data:
            self._buf += data
            self._last_rx = now
            events += self._scan(final=False)
        return events

    def check_timeout(self, now: float | None = None) -> list[ParseEvent]:
        """데이터가 없을 때도 주기적으로 불러서 끊긴 패킷을 정리한다."""
        now = self._clock() if now is None else now
        if self._buf and now - self._last_rx > self._timeout:
            return self._scan(final=True)
        return []

    # ------------------------------------------------------------ 내부
    def _scan(self, final: bool) -> list[ParseEvent]:
        """
        버퍼 앞에서부터 패킷을 꺼낸다.
        final=True 이면 더 기다리지 않는다: 미완성 헤더는 SOF 한 바이트씩 버려 가며
        뒤에 숨어 있는 정상 패킷을 찾는다.
        """
        events: list[ParseEvent] = []
        dropped = bytearray()
        buf = self._buf

        def flush_dropped() -> None:
            if not dropped:
                return
            if final:
                msg = f"수신이 끊겨 미완성 데이터 {len(dropped)}바이트를 버림"
                events.append(ParseError("timeout", bytes(dropped), msg))
            else:
                msg = f"패킷이 아닌 데이터 {len(dropped)}바이트를 버림"
                events.append(ParseError("garbage", bytes(dropped), msg))
            dropped.clear()

        while buf:
            # 1) SOF 찾기
            if buf[0] != SOF:
                idx = buf.find(SOF)
                cut = len(buf) if idx < 0 else idx
                dropped += buf[:cut]
                del buf[:cut]
                continue

            # 2) LEN 이 왔는지
            if len(buf) < 2:
                if not final:
                    break
                dropped.append(buf[0])
                del buf[0]
                continue

            length = buf[1]                      # CMD 1바이트 + PAYLOAD
            if length == 0 or length > self._max_len:   # 있을 수 없는 길이 -> 가짜 SOF
                dropped.append(buf[0])
                del buf[0]
                continue

            # 3) 패킷 전체가 다 왔는지 (SOF + LEN + [CMD + PAYLOAD] + CHECKSUM)
            total = length + 3
            if len(buf) < total:
                if not final:
                    break
                dropped.append(buf[0])
                del buf[0]
                continue

            # 4) 체크섬 확인
            cmd = buf[2]
            data = bytes(buf[3:2 + length])
            received = buf[total - 1]
            expected = checksum(cmd, data)
            flush_dropped()
            if received == expected:
                events.append(Packet(cmd, data))
                del buf[:total]
            else:
                events.append(ParseError(
                    "checksum",
                    bytes(buf[:total]),
                    f"체크섬 오류 (계산 0x{expected:02X}, 수신 0x{received:02X})",
                ))
                del buf[0]                       # SOF 다음 바이트부터 다시 찾기

        flush_dropped()
        return events
