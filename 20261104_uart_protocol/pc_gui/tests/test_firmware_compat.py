"""
보드 펌웨어(20261104_uart_protocol)의 C 코드를 그대로 옮긴 모델로 호환성을 확인한다.

  uart_protocol.c : rx_parse_byte(), UART_protocol_send()
  main.c          : packet_do_func()

PC 가 보낸 패킷을 펌웨어가 받아들이는지, 펌웨어가 보낸 응답을 PC 가 해석하는지 본다.
펌웨어 코드를 바꾸면 이 모델도 같이 바꿔야 한다.
"""
import protocol as P
from packet_parser import PacketParser

PKT_STATE = 0xAA
PKT_MAX_LEN = 16
RX_WAIT_START, RX_WAIT_LEN, RX_WAIT_DATA, RX_WAIT_CHK = range(4)


class FirmwareModel:
    def __init__(self):
        self.rx_state = RX_WAIT_START
        self.rx_len = 0
        self.rx_idx = 0
        self.rx_buf = [0] * PKT_MAX_LEN
        self.tx = bytearray()
        # GPIO: PB15, PB14, PB13, PC4 (LED0~3)
        self.leds = [0, 0, 0, 0]

    # ---- uart_protocol.c: rx_parse_byte
    def rx_parse_byte(self, rx_data):
        if self.rx_state == RX_WAIT_START:
            if rx_data == PKT_STATE:
                self.rx_state = RX_WAIT_LEN
        elif self.rx_state == RX_WAIT_LEN:
            if rx_data == 0 or rx_data >= PKT_MAX_LEN:
                self.rx_state = RX_WAIT_START
            else:
                self.rx_len = rx_data
                self.rx_idx = 0
                self.rx_state = RX_WAIT_DATA
        elif self.rx_state == RX_WAIT_DATA:
            self.rx_buf[self.rx_idx] = rx_data
            self.rx_idx += 1
            if self.rx_idx >= self.rx_len:
                self.rx_state = RX_WAIT_CHK
        elif self.rx_state == RX_WAIT_CHK:
            cksum = self.rx_len
            for i in range(self.rx_len):
                cksum = (cksum + self.rx_buf[i]) & 0xFF
            if cksum == rx_data:
                self.packet_do_func(self.rx_buf[0], self.rx_buf[1:self.rx_len], self.rx_len - 1)
            self.rx_state = RX_WAIT_START

    # ---- uart_protocol.c: UART_protocol_send
    def send(self, cmd, payload):
        length = len(payload) + 1
        cksum = (length + cmd + sum(payload)) & 0xFF
        self.tx += bytes([PKT_STATE, length, cmd, *payload, cksum])

    # ---- main.c: packet_do_func
    def packet_do_func(self, cmd, payload, payload_len):
        if 0x10 <= cmd <= 0x13:
            if payload_len >= 1:
                self.leds[cmd - 0x10] = 1 if payload[0] == 1 else 0
                rsp = P.RSP_OK
            else:
                rsp = P.RSP_ERR
            self.send(cmd + 0x80, [rsp])
        elif cmd == 0x14:
            self.send(cmd + 0x80, list(self.leds))
        elif cmd == 0x20:
            self.send(cmd + 0x80, [21, 1, 48, 0])
        elif cmd == 0x30:
            dummy_distance = 0x01FA
            self.send(cmd + 0x80, [dummy_distance >> 8, dummy_distance & 0xFF])

    def receive(self, data: bytes) -> bytes:
        for b in data:
            self.rx_parse_byte(b)
        out, self.tx = bytes(self.tx), bytearray()
        return out


def roundtrip(fw, req):
    raw = fw.receive(req.to_bytes())
    assert raw, f"펌웨어가 {P.describe_request(req)} 에 응답하지 않음"
    events = PacketParser().feed(raw, now=0)
    assert len(events) == 1 and isinstance(events[0], P.Packet), events
    pkt = events[0]
    assert pkt.cmd in req.expects
    return P.parse_response(pkt)


def test_every_request_gets_a_valid_response():
    fw = FirmwareModel()
    for i in range(4):
        assert roundtrip(fw, P.led_set(i, True)) == P.LedResult(True)
    assert fw.leds == [1, 1, 1, 1]
    assert roundtrip(fw, P.led_set(2, False)) == P.LedResult(True)
    assert roundtrip(fw, P.led_status_req()).states == (True, True, False, True)
    th = roundtrip(fw, P.temp_humid_req())
    assert (round(th.temp_c, 1), round(th.humid_pct, 1)) == (21.1, 48.0)
    assert roundtrip(fw, P.distance_req()).cm == 506


def test_firmware_ignores_old_cmd_first_format():
    """오늘 처음 만든 PC 프로그램(CMD 가 LEN 앞)의 패킷은 펌웨어가 전부 버린다 -> RX 0 의 원인."""
    fw = FirmwareModel()
    old_format = [
        bytes.fromhex("AA 14 00 14"),        # LED_STATUS_REQ
        bytes.fromhex("AA 10 01 01 12"),     # LED0 ON
        bytes.fromhex("AA 20 00 20"),        # TEMP_HUMID_REQ
        bytes.fromhex("AA 30 00 30"),        # DISTANCE_REQ
    ]
    for raw in old_format:
        assert fw.receive(raw) == b""
    assert fw.leds == [0, 0, 0, 0]


def test_firmware_recovers_after_noise():
    fw = FirmwareModel()
    fw.receive(b"\x00\x13\xFF")
    assert roundtrip(fw, P.led_status_req()).ok
