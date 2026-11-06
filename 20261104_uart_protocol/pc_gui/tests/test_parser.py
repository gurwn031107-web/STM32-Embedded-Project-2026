"""패킷 파서: 분할 수신, 노이즈, 체크섬 오류 재동기화, 수신 타임아웃."""
from packet_parser import PacketParser, ParseError
from protocol import Packet, build_packet

TH = build_packet(0xA0, bytes([21, 1, 48, 0]))
DIST = build_packet(0xB0, bytes([0x01, 0xFA]))
TH_PKT = Packet(0xA0, bytes([21, 1, 48, 0]))
DIST_PKT = Packet(0xB0, bytes([0x01, 0xFA]))


def packets(events):
    return [e for e in events if isinstance(e, Packet)]


def errors(events, kind=None):
    return [e for e in events if isinstance(e, ParseError) and (kind is None or e.kind == kind)]


def test_single_packet():
    p = PacketParser()
    assert packets(p.feed(TH, now=0)) == [TH_PKT]
    assert p.pending_bytes == 0


def test_request_without_payload():
    p = PacketParser()
    assert packets(p.feed(build_packet(0x14), now=0)) == [Packet(0x14, b"")]


def test_packet_split_byte_by_byte():
    p = PacketParser()
    out = []
    for i, b in enumerate(TH):
        out += p.feed(bytes([b]), now=i * 0.001)
    assert packets(out) == [TH_PKT]
    assert errors(out) == []


def test_two_packets_in_one_chunk():
    p = PacketParser()
    assert packets(p.feed(TH + DIST, now=0)) == [TH_PKT, DIST_PKT]


def test_garbage_before_packet_is_dropped():
    p = PacketParser()
    ev = p.feed(b"\x00\x13\x37" + DIST, now=0)
    assert packets(ev) == [DIST_PKT]
    assert errors(ev, "garbage")[0].raw == b"\x00\x13\x37"


def test_checksum_error_resyncs_on_aa_inside_payload():
    broken = bytes([0xAA, 0x03, 0xB0, 0xAA, 0x00, 0x99])      # 체크섬 틀림, PAYLOAD 에 0xAA
    p = PacketParser()
    ev = p.feed(broken + TH, now=0)
    assert errors(ev, "checksum")
    assert packets(ev) == [TH_PKT]


def test_valid_packet_with_aa_in_payload():
    pkt = build_packet(0xB0, bytes([0x00, 0xAA]))              # 170cm
    p = PacketParser()
    assert packets(p.feed(pkt, now=0)) == [Packet(0xB0, bytes([0, 0xAA]))]


def test_impossible_length_is_treated_as_fake_sof():
    p = PacketParser()
    ev = p.feed(b"\xAA\x55\x10" + TH, now=0)       # LEN=0x55 는 있을 수 없음
    assert packets(ev) == [TH_PKT]
    ev = p.feed(b"\xAA\x00" + DIST, now=0)         # LEN=0 도 있을 수 없음 (CMD 가 최소 1바이트)
    assert packets(ev) == [DIST_PKT]


def test_incomplete_packet_dropped_after_inter_byte_timeout():
    p = PacketParser(inter_byte_timeout=0.05)
    assert p.feed(TH[:4], now=0.0) == []
    ev = p.check_timeout(now=0.2)
    assert errors(ev, "timeout") and p.pending_bytes == 0
    assert packets(p.feed(DIST, now=0.3)) == [DIST_PKT]


def test_timeout_recovers_packet_hidden_behind_fake_header():
    # 가짜 SOF(LEN=10) 때문에 기다리는 동안 뒤에 진짜 패킷이 들어와 있는 상황
    p = PacketParser(inter_byte_timeout=0.05)
    assert packets(p.feed(b"\xAA\x0A" + TH, now=0.0)) == []
    ev = p.check_timeout(now=0.2)
    assert packets(ev) == [TH_PKT]


def test_gap_inside_packet_restarts_on_next_feed():
    p = PacketParser(inter_byte_timeout=0.05)
    p.feed(TH[:3], now=0.0)
    ev = p.feed(DIST, now=1.0)                      # 1초 뒤 새 패킷
    assert errors(ev, "timeout")
    assert packets(ev) == [DIST_PKT]
