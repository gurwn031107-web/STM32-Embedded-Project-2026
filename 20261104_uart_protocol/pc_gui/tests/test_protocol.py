"""프로토콜: 체크섬, 패킷 생성, 응답 해석 (보드 펌웨어 형식 기준)."""
import pytest

import protocol as P


def h(s: str) -> bytes:
    return bytes.fromhex(s)


def test_request_packets():
    # | AA | LEN(=1+PAYLOAD) | CMD | PAYLOAD | CHK(=LEN+CMD+PAYLOAD) |
    assert P.led_set(0, True).to_bytes() == h("AA 02 10 01 13")
    assert P.led_set(3, False).to_bytes() == h("AA 02 13 00 15")
    assert P.led_status_req().to_bytes() == h("AA 01 14 15")
    assert P.temp_humid_req().to_bytes() == h("AA 01 20 21")
    assert P.distance_req().to_bytes() == h("AA 01 30 31")


def test_firmware_response_packets():
    # 펌웨어 UART_protocol_send 가 만드는 바이트와 같아야 한다
    assert P.build_packet(0x90, b"\x00") == h("AA 02 90 00 92")
    assert P.build_packet(0x94, bytes([1, 0, 0, 1])) == h("AA 05 94 01 00 00 01 9B")
    assert P.build_packet(0xA0, bytes([21, 1, 48, 0])) == h("AA 05 A0 15 01 30 00 EB")
    assert P.build_packet(0xB0, bytes([0x01, 0xFA])) == h("AA 03 B0 01 FA AE")


def test_checksum_wraps_to_8_bits():
    payload = bytes([0xFF, 0xFF, 0xFF])
    assert P.checksum(0xB0, payload) == (4 + 0xB0 + 0xFF * 3) & 0xFF


def test_payload_limit_matches_firmware():
    P.build_packet(0x10, bytes(14))                  # LEN = 15 까지 허용
    with pytest.raises(ValueError):
        P.build_packet(0x10, bytes(15))              # LEN = 16 은 펌웨어가 버림


def test_led_requests_accept_firmware_and_common_response_cmd():
    for i in range(4):
        r = P.led_set(i, i % 2 == 0)
        assert r.cmd == 0x10 + i
        assert r.expects == {0x90 + i, 0x90}
        assert r.led_index == i
        assert r.led_on is (i % 2 == 0)
    with pytest.raises(ValueError):
        P.led_set(4, True)


def test_parse_firmware_responses():
    for c in (0x90, 0x91, 0x92, 0x93):
        assert P.parse_response(P.Packet(c, b"\x00")) == P.LedResult(True)
        assert P.parse_response(P.Packet(c, b"\x01")) == P.LedResult(False)
    st = P.parse_response(P.Packet(0x94, bytes([1, 0, 1, 0])))
    assert st.ok and st.states == (True, False, True, False)
    th = P.parse_response(P.Packet(0xA0, bytes([21, 1, 48, 0])))
    assert th.ok and th.temp_c == pytest.approx(21.1) and th.humid_pct == pytest.approx(48.0)
    d = P.parse_response(P.Packet(0xB0, bytes([0x01, 0xFA])))
    assert d.ok and d.cm == 506                      # 빅엔디안


def test_single_err_byte_means_sensor_failure():
    assert P.parse_response(P.Packet(0xA0, b"\x01")) == P.TempHumid(False)
    assert P.parse_response(P.Packet(0xB0, b"\x01")) == P.Distance(False)
    assert P.parse_response(P.Packet(0x94, b"\x01")) == P.LedStatus(False)


@pytest.mark.parametrize("cmd,data", [
    (0xA0, bytes([0, 24, 41])),             # 예전 형식 (결과 바이트 + 값 2개)
    (0xB0, bytes([0, 0x84, 0x00])),         # 예전 형식
    (0x94, bytes([0, 0b0101])),             # 예전 비트마스크 형식
    (0x90, b""),                            # 결과 바이트 없음
    (0x91, b"\x07"),                        # 모르는 결과 코드
])
def test_format_mismatch_is_reported(cmd, data):
    with pytest.raises(P.ProtocolError):
        P.parse_response(P.Packet(cmd, data))


def test_describe_for_log():
    assert P.describe_request(P.led_set(2, False)) == "LED2_SET OFF"
    assert P.describe_packet(P.Packet(0xA0, bytes([21, 1, 48, 0]))) == "TEMP_HUMID_RSP 21.1°C 48.0%"
    assert P.describe_packet(P.Packet(0x92, b"\x00")) == "LED2_RSP OK"
    assert "해석 실패" in P.describe_packet(P.Packet(0xB0, bytes([0, 1, 2])))
    assert P.hexdump(h("AA 01 14 15")) == "AA 01 14 15"
