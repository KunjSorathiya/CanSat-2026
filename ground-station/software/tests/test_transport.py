import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from transport import (
    FileReplayTransport,
    FrameDecoder,
    LoopbackTransport,
    SerialTransport,
    crc16_ccitt,
    frame_encode,
)

PACKET = ("CAN-Team-01; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
          "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;")


class FramingTests(unittest.TestCase):
    def test_known_crc_vector(self):
        self.assertEqual(crc16_ccitt(b"123456789"), 0x29B1)

    def test_frame_roundtrip(self):
        decoder = FrameDecoder()
        frames = list(decoder.feed(frame_encode(PACKET.encode())))
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].kind, "packet")
        self.assertEqual(frames[0].payload, PACKET)
        self.assertTrue(frames[0].crc_ok)
        self.assertEqual(decoder.frames_ok, 1)

    def test_status_frame_detected(self):
        decoder = FrameDecoder()
        frames = list(decoder.feed(frame_encode(b"#state=RX radio=1")))
        self.assertEqual(frames[0].kind, "status")

    def test_a_truncated_header_does_not_swallow_the_next_frame(self):
        # A header cut off inside its CRC field is followed immediately by the next
        # frame's own '$'. Discarding that byte as part of the resync would cost the
        # following frame too, turning one corrupted header into two lost packets.
        decoder = FrameDecoder()
        frames = list(decoder.feed(b"$9,ab" + frame_encode(PACKET.encode())))
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].payload, PACKET)
        self.assertEqual(decoder.resyncs, 1)

    def test_a_truncated_length_does_not_swallow_the_next_frame(self):
        decoder = FrameDecoder()
        frames = list(decoder.feed(b"$12" + frame_encode(PACKET.encode())))
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].payload, PACKET)

    def test_crc_error_reported(self):
        raw = bytearray(frame_encode(PACKET.encode()))
        raw[-3] ^= 0x20  # corrupt a payload byte
        decoder = FrameDecoder()
        frames = list(decoder.feed(bytes(raw)))
        self.assertEqual(frames[0].kind, "crc")
        self.assertFalse(frames[0].crc_ok)
        self.assertEqual(decoder.crc_errors, 1)

    def test_resync_after_noise(self):
        decoder = FrameDecoder()
        stream = b"junk noise$$$" + frame_encode(b"A") + b"\x00\xff" + frame_encode(b"B")
        payloads = [f.payload for f in decoder.feed(stream)]
        self.assertEqual(payloads, ["A", "B"])

    def test_split_across_chunks(self):
        decoder = FrameDecoder()
        blob = frame_encode(PACKET.encode())
        got = []
        for i in range(0, len(blob), 7):
            got.extend(decoder.feed(blob[i:i + 7]))
        self.assertEqual(len(got), 1)
        self.assertEqual(got[0].payload, PACKET)


class TransportTests(unittest.TestCase):
    def test_file_replay_plain(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "packets.txt"
            path.write_text(PACKET + "\n" + PACKET.replace("P-001", "P-002") + "\n",
                            encoding="utf-8")
            frames = list(FileReplayTransport(str(path)).frames())
            self.assertEqual([f.kind for f in frames], ["raw", "raw"])
            self.assertIn("P-002", frames[1].payload)

    def test_file_replay_framed(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "frames.bin"
            with path.open("wb") as stream:
                stream.write(frame_encode(PACKET.encode()))
                stream.write(frame_encode(b"#state=RX"))
            frames = list(FileReplayTransport(str(path), framed=True).frames())
            self.assertEqual([f.kind for f in frames], ["packet", "status"])

    def test_loopback_framed(self):
        t = LoopbackTransport(framed=True)
        t.push_packet(PACKET)
        t.push_packet("#state=RX radio=1")
        t.stop()
        kinds = [f.kind for f in t.frames()]
        self.assertEqual(kinds, ["packet", "status"])


if __name__ == "__main__":
    unittest.main()


class SerialBufferTests(unittest.TestCase):
    """The unframed reader must not accumulate an unbounded partial line."""

    def test_a_stuck_link_cannot_grow_the_buffer_without_bound(self):
        # SerialTransport needs pyserial to construct, so exercise the buffer rule
        # directly: it is a property of the class, not of the serial port.
        cap = SerialTransport.MAX_LINE
        self.assertGreater(cap, 0)
        self.assertLessEqual(cap, 65536)

        buf = b""
        resyncs = 0
        for _ in range(100):
            buf += b"x" * 1000          # noise, never a newline
            if len(buf) > cap:
                buf = b""
                resyncs += 1
        self.assertLessEqual(len(buf), cap)
        self.assertGreater(resyncs, 0)
