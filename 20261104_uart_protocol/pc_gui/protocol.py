"""
바이너리 패킷 프로토콜 (보드 펌웨어 uart_protocol.c 와 같은 형식).

    | SOF(0xAA) | LEN | CMD | PAYLOAD (LEN-1 바이트) | CHECKSUM |

    LEN      = CMD 1바이트 + PAYLOAD 길이  (1 ~ 15, 펌웨어 PKT_MAX_LEN=16 미만)
    CHECKSUM = (LEN + CMD + PAYLOAD 합) & 0xFF

응답 PAYLOAD (펌웨어 packet_do_func 기준)
    LED0~3 응답     (0x90~0x93 = 요청 CMD + 0x80) : [결과 RSP_OK / RSP_ERR]
    LED_STATUS_RSP  (0x94) : [LED0, LED1, LED2, LED3]  각 0 = OFF, 1 = ON
    TEMP_HUMID_RSP  (0xA0) : [온도 정수, 온도 소수, 습도 정수, 습도 소수]
    DISTANCE_RSP    (0xB0) : [거리 상위바이트, 거리 하위바이트]  (cm, 빅엔디안)

센서 응답이 [RSP_ERR] 한 바이트로 오면 측정 실패로 처리한다
(현재 펌웨어는 보내지 않지만, 센서 오류 처리를 넣을 때 쓸 수 있도록).
"""
from __future__ import annotations

from dataclasses import dataclass

SOF = 0xAA
MAX_LEN = 15                 # LEN 필드 최댓값 (펌웨어: LEN >= PKT_MAX_LEN(16) 이면 버림)

# ---------------------------------------------------------------- 요청 CMD (PC -> STM32)
CMD_LED0_SET = 0x10
CMD_LED1_SET = 0x11
CMD_LED2_SET = 0x12
CMD_LED3_SET = 0x13
CMD_LED_STATUS_REQ = 0x14
CMD_TEMP_HUMID_REQ = 0x20
CMD_DISTANCE_REQ = 0x30

# ---------------------------------------------------------------- 응답 CMD (STM32 -> PC)
CMD_LED_RSP = 0x90           # LED0 응답. 펌웨어는 LEDn 응답을 0x90 + n 으로 보낸다
CMD_LED_STATUS_RSP = 0x94
CMD_TEMP_HUMID_RSP = 0xA0
CMD_DISTANCE_RSP = 0xB0

RSP_OK = 0x00
RSP_ERR = 0x01

LED_COUNT = 4
LED_SET_CMDS = (CMD_LED0_SET, CMD_LED1_SET, CMD_LED2_SET, CMD_LED3_SET)
LED_RSP_CMDS = frozenset(c + 0x80 for c in LED_SET_CMDS)          # 0x90 ~ 0x93

# 요청 CMD -> 받아들일 응답 CMD
# LEDn 은 펌웨어대로 0x90+n 이 기본이고, 공용 0x90(CMD_LED_RSP)으로 와도 받아준다.
# 요청을 하나씩 보내므로 어느 LED 의 응답인지 헷갈리지 않는다.
RESPONSE_FOR = {
    **{c: frozenset({c + 0x80, CMD_LED_RSP}) for c in LED_SET_CMDS},
    CMD_LED_STATUS_REQ: frozenset({CMD_LED_STATUS_RSP}),
    CMD_TEMP_HUMID_REQ: frozenset({CMD_TEMP_HUMID_RSP}),
    CMD_DISTANCE_REQ: frozenset({CMD_DISTANCE_RSP}),
}
RESPONSE_CMDS = frozenset().union(*RESPONSE_FOR.values())

CMD_NAMES = {
    CMD_LED0_SET: "LED0_SET",
    CMD_LED1_SET: "LED1_SET",
    CMD_LED2_SET: "LED2_SET",
    CMD_LED3_SET: "LED3_SET",
    CMD_LED_STATUS_REQ: "LED_STATUS_REQ",
    CMD_TEMP_HUMID_REQ: "TEMP_HUMID_REQ",
    CMD_DISTANCE_REQ: "DISTANCE_REQ",
    0x90: "LED0_RSP",
    0x91: "LED1_RSP",
    0x92: "LED2_RSP",
    0x93: "LED3_RSP",
    CMD_LED_STATUS_RSP: "LED_STATUS_RSP",
    CMD_TEMP_HUMID_RSP: "TEMP_HUMID_RSP",
    CMD_DISTANCE_RSP: "DISTANCE_RSP",
}


class ProtocolError(ValueError):
    """패킷은 정상 수신했지만 PAYLOAD 형식이 약속과 다를 때."""


# ================================================================ 패킷 만들기
def checksum(cmd: int, payload: bytes) -> int:
    length = len(payload) + 1
    return (length + cmd + sum(payload)) & 0xFF


def build_packet(cmd: int, payload: bytes = b"") -> bytes:
    payload = bytes(payload)
    if not 0 <= cmd <= 0xFF:
        raise ValueError(f"CMD 범위 초과: {cmd}")
    length = len(payload) + 1
    if length > MAX_LEN:
        raise ValueError(f"PAYLOAD 가 너무 김: {len(payload)} 바이트 (최대 {MAX_LEN - 1})")
    return bytes([SOF, length, cmd]) + payload + bytes([checksum(cmd, payload)])


@dataclass(frozen=True)
class Packet:
    """체크섬까지 통과한 수신 패킷."""
    cmd: int
    data: bytes          # PAYLOAD (CMD 제외)

    def to_bytes(self) -> bytes:
        return build_packet(self.cmd, self.data)


@dataclass(frozen=True)
class Request:
    """PC 가 보내는 요청 하나."""
    cmd: int
    data: bytes = b""

    @property
    def expects(self) -> frozenset:
        """이 요청에 대한 응답으로 받아들일 CMD 들."""
        return RESPONSE_FOR[self.cmd]

    @property
    def led_index(self) -> int | None:
        return self.cmd - CMD_LED0_SET if self.cmd in LED_SET_CMDS else None

    @property
    def led_on(self) -> bool | None:
        if self.led_index is None or not self.data:
            return None
        return self.data[0] == 1

    def to_bytes(self) -> bytes:
        return build_packet(self.cmd, self.data)


def led_set(index: int, on: bool) -> Request:
    if not 0 <= index < LED_COUNT:
        raise ValueError(f"LED 번호는 0~{LED_COUNT - 1}: {index}")
    return Request(CMD_LED0_SET + index, bytes([1 if on else 0]))


def led_status_req() -> Request:
    return Request(CMD_LED_STATUS_REQ)


def temp_humid_req() -> Request:
    return Request(CMD_TEMP_HUMID_REQ)


def distance_req() -> Request:
    return Request(CMD_DISTANCE_REQ)


# ================================================================ 응답 해석
@dataclass(frozen=True)
class LedResult:
    ok: bool


@dataclass(frozen=True)
class LedStatus:
    ok: bool
    states: tuple[bool, ...] | None = None   # (LED0, LED1, LED2, LED3)


@dataclass(frozen=True)
class TempHumid:
    ok: bool
    temp_c: float | None = None
    humid_pct: float | None = None


@dataclass(frozen=True)
class Distance:
    ok: bool
    cm: int | None = None


Response = LedResult | LedStatus | TempHumid | Distance


def _bad_len(packet: Packet, expected: str) -> ProtocolError:
    return ProtocolError(
        f"{cmd_name(packet.cmd)} PAYLOAD 길이 불일치: {expected}바이트 예상, "
        f"{len(packet.data)}바이트 수신 ({hexdump(packet.data) or '비어 있음'})"
    )


def _is_err_only(d: bytes) -> bool:
    return len(d) == 1 and d[0] == RSP_ERR


def parse_response(packet: Packet) -> Response:
    """응답 패킷의 PAYLOAD 를 해석한다. 형식이 약속과 다르면 ProtocolError."""
    d = packet.data
    cmd = packet.cmd

    if cmd in LED_RSP_CMDS:
        if len(d) != 1:
            raise _bad_len(packet, "1")
        if d[0] not in (RSP_OK, RSP_ERR):
            raise ProtocolError(f"{cmd_name(cmd)} 알 수 없는 결과 코드 0x{d[0]:02X}")
        return LedResult(d[0] == RSP_OK)

    if cmd == CMD_LED_STATUS_RSP:
        if _is_err_only(d):
            return LedStatus(False)
        if len(d) != LED_COUNT:
            raise _bad_len(packet, str(LED_COUNT))
        return LedStatus(True, tuple(b != 0 for b in d))

    if cmd == CMD_TEMP_HUMID_RSP:
        if _is_err_only(d):
            return TempHumid(False)
        if len(d) != 4:
            raise _bad_len(packet, "4")
        t_int, t_dec, h_int, h_dec = d
        return TempHumid(True, t_int + t_dec / 10, h_int + h_dec / 10)

    if cmd == CMD_DISTANCE_RSP:
        if _is_err_only(d):
            return Distance(False)
        if len(d) != 2:
            raise _bad_len(packet, "2")
        return Distance(True, (d[0] << 8) | d[1])

    raise ProtocolError(f"응답 CMD 가 아님: 0x{cmd:02X}")


# ================================================================ 로그 표시용
def cmd_name(cmd: int) -> str:
    return CMD_NAMES.get(cmd, f"CMD 0x{cmd:02X}")


def hexdump(raw: bytes) -> str:
    return " ".join(f"{b:02X}" for b in raw)


def describe_request(req: Request) -> str:
    if req.led_index is not None:
        return f"{cmd_name(req.cmd)} {'ON' if req.led_on else 'OFF'}"
    return cmd_name(req.cmd)


def describe_response(resp: Response) -> str:
    if not resp.ok:
        return "ERR"
    if isinstance(resp, LedStatus):
        return " ".join(f"LED{i}={'ON' if s else 'OFF'}" for i, s in enumerate(resp.states))
    if isinstance(resp, TempHumid):
        return f"{resp.temp_c:.1f}°C {resp.humid_pct:.1f}%"
    if isinstance(resp, Distance):
        return f"{resp.cm} cm"
    return "OK"


def describe_packet(packet: Packet) -> str:
    name = cmd_name(packet.cmd)
    if packet.cmd in RESPONSE_CMDS:
        try:
            return f"{name} {describe_response(parse_response(packet))}"
        except ProtocolError as e:
            return f"{name} (해석 실패: {e})"
    if packet.cmd in RESPONSE_FOR:
        return describe_request(Request(packet.cmd, packet.data))
    return name
