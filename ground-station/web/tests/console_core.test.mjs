/**
 * Tests for the web console's portable core.
 *
 * ground-station/web/index.html is a single self-contained file with no build step, so its
 * framing, parser, validator and link-health logic are hand-ports of the C++ and Python
 * implementations. Ports drift. This harness extracts the code between the
 * PORTABLE-CORE markers straight out of the HTML and runs it under Node, so a divergence
 * fails the build instead of surfacing during a mission.
 *
 *   node --test ground-station/web/tests/
 *
 * The parser cases come from test-data/protocol-fixtures.tsv — the same file the C++ and
 * Python suites read, so all three are held to one definition of a valid packet.
 */

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import assert from "node:assert/strict";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = join(HERE, "..", "..", "..");
const CONSOLE_HTML = join(REPO_ROOT, "ground-station", "web", "index.html");
const FIXTURES = join(REPO_ROOT, "test-data", "protocol-fixtures.tsv");

const BEGIN = "// PORTABLE-CORE:BEGIN";
const END = "// PORTABLE-CORE:END";

function loadConsoleCore() {
  const html = readFileSync(CONSOLE_HTML, "utf8");
  const afterBegin = html.split(BEGIN);
  assert.equal(afterBegin.length, 2, `expected exactly one ${BEGIN} marker in index.html`);
  const parts = afterBegin[1].split(END);
  assert.equal(parts.length, 2, `expected exactly one ${END} marker in index.html`);
  const source = parts[0];

  // The core must not reach for the DOM or browser-only globals: that is what makes it
  // testable here, and what keeps the mission logic separable from the presentation.
  for (const forbidden of ["document.", "window.", "matchMedia(", "localStorage"]) {
    assert.ok(
      !source.includes(forbidden),
      `portable core must not use ${forbidden} — move it below ${END}`
    );
  }

  const factory = new Function(
    `${source}\nreturn { crc16ccitt, frameEncode, FrameDecoder, parsePacket, StreamValidator, LinkHealth, RATE_WINDOW_S };`
  );
  return factory();
}

function loadFixtures() {
  const text = readFileSync(FIXTURES, "utf8");
  const cases = [];
  for (const rawLine of text.split("\n")) {
    const line = rawLine.replace(/\r$/, "");
    if (!line.trim() || line.startsWith("#")) continue;
    const first = line.indexOf("\t");
    const second = line.indexOf("\t", first + 1);
    assert.ok(first > 0 && second > first, `malformed fixture line: ${line}`);
    cases.push({
      id: line.slice(0, first),
      expect: line.slice(first + 1, second),
      packet: line.slice(second + 1),
    });
  }
  assert.ok(cases.length >= 30, `expected the full fixture set, got ${cases.length}`);
  return cases;
}

const M = loadConsoleCore();
const FIXTURE_CASES = loadFixtures();

/* ---------------------------------------------------------------- framing */

test("crc16 matches the known-answer vector shared with the firmware", () => {
  const bytes = new TextEncoder().encode("123456789");
  assert.equal(M.crc16ccitt(bytes), 0x29b1);
});

test("crc16 of an empty payload is the initial value", () => {
  assert.equal(M.crc16ccitt(new Uint8Array(0)), 0xffff);
});

test("a frame round-trips through the decoder", () => {
  const decoder = new M.FrameDecoder();
  const frames = [...decoder.feed(M.frameEncode("hello bridge"))];
  assert.equal(frames.length, 1);
  assert.equal(frames[0].payload, "hello bridge");
  assert.equal(frames[0].kind, "packet");
  assert.equal(frames[0].crcOk, true);
  assert.equal(decoder.framesOk, 1);
});

test("a payload starting with # is classified as a status frame", () => {
  const decoder = new M.FrameDecoder();
  const frames = [...decoder.feed(M.frameEncode("#state=RX radio=1"))];
  assert.equal(frames[0].kind, "status");
});

test("a corrupted payload is reported as a CRC error, not silently accepted", () => {
  const bytes = M.frameEncode("CAN-Team-07; P-001;");
  bytes[bytes.length - 3] ^= 0xff; // flip a payload bit
  const decoder = new M.FrameDecoder();
  const frames = [...decoder.feed(bytes)];
  assert.equal(frames.length, 1);
  assert.equal(frames[0].kind, "crc");
  assert.equal(frames[0].crcOk, false);
  assert.equal(decoder.crcErrors, 1);
  assert.equal(decoder.framesOk, 0);
});

test("the decoder resynchronises after leading noise", () => {
  const decoder = new M.FrameDecoder();
  const noise = [...new TextEncoder().encode("garbage\x00\xff$$$")];
  const frames = [...decoder.feed(noise.concat(M.frameEncode("after noise")))];
  assert.equal(frames.length, 1);
  assert.equal(frames[0].payload, "after noise");
});

test("a truncated header does not swallow the next frame", () => {
  // A header cut off inside its CRC field is followed immediately by the next frame's
  // own '$'. Discarding that byte as part of the resync would cost the following frame
  // too, turning one corrupted header into two lost packets. Mirrors framing.cpp.
  const decoder = new M.FrameDecoder();
  const noise = [...new TextEncoder().encode("$9,ab")];
  const frames = [...decoder.feed(noise.concat(M.frameEncode("after a torn header")))];
  assert.equal(frames.length, 1);
  assert.equal(frames[0].payload, "after a torn header");
  assert.equal(decoder.resyncs, 1);
});

test("a truncated length field does not swallow the next frame", () => {
  const decoder = new M.FrameDecoder();
  const noise = [...new TextEncoder().encode("$12")];
  const frames = [...decoder.feed(noise.concat(M.frameEncode("after a torn length")))];
  assert.equal(frames.length, 1);
  assert.equal(frames[0].payload, "after a torn length");
});

test("a frame split across chunks is reassembled", () => {
  const bytes = M.frameEncode("split across reads");
  const decoder = new M.FrameDecoder();
  const first = [...decoder.feed(bytes.slice(0, 5))];
  assert.equal(first.length, 0);
  const rest = [...decoder.feed(bytes.slice(5))];
  assert.equal(rest.length, 1);
  assert.equal(rest[0].payload, "split across reads");
});

test("an oversized length field is rejected rather than buffered", () => {
  const decoder = new M.FrameDecoder();
  const frames = [...decoder.feed([...new TextEncoder().encode("$99999,ffff,")])];
  assert.equal(frames.length, 0);
  assert.ok(decoder.resyncs > 0);
});

test("a UTF-8 payload survives the round trip", () => {
  const decoder = new M.FrameDecoder();
  const frames = [...decoder.feed(M.frameEncode("altitude ±5 m — ok"))];
  assert.equal(frames[0].payload, "altitude ±5 m — ok");
});

/* ----------------------------------------------------------------- parser */

test("every shared fixture parses to the same verdict as the C++ and Python parsers", () => {
  for (const { id, expect, packet } of FIXTURE_CASES) {
    const result = M.parsePacket(packet, null);
    const verdict = result.record ? "ok" : "err";
    assert.equal(verdict, expect, `fixture ${id}: expected ${expect}, got ${verdict}`);
  }
});

test("the canonical packet yields the documented field values", () => {
  const canonical = FIXTURE_CASES.find((c) => c.id === "canonical");
  const { record } = M.parsePacket(canonical.packet, null);
  assert.equal(record.team_id, "CAN-Team-07");
  assert.equal(record.packet_number, 1);
  assert.equal(record.timestamp, "00:00:01:000");
  assert.equal(record.timestamp_ms, 1000);
  assert.equal(record.altitude, 10.0);
  assert.equal(record.pressure, 101325.0);
  assert.equal(record.temperature, 25.0);
  assert.equal(record.roll, 1.0);
  assert.equal(record.pitch, 2.0);
  assert.equal(record.yaw, 3.0);
  assert.equal(record.ax, 0.1);
  assert.equal(record.ay, 0.2);
  assert.equal(record.az, 9.8);
  assert.equal(record.gps_lat, null);
  assert.equal(record.mode, null);
});

test("optional GPS and diagnostic fields are read, not ignored", () => {
  const fixture = FIXTURE_CASES.find((c) => c.id === "gps_and_diagnostics");
  const { record } = M.parsePacket(fixture.packet, null);
  assert.equal(record.gps_lat, 18.0);
  assert.equal(record.gps_lon, 73.0);
  assert.equal(record.gps_alt, 20.0);
  assert.equal(record.mode, "READY");
  assert.equal(record.faults, 0);
});

test("all four diagnostic tags are decoded", () => {
  const fixture = FIXTURE_CASES.find((c) => c.id === "diagnostics_only");
  const { record } = M.parsePacket(fixture.packet, null);
  assert.equal(record.mode, "FLIGHT");
  assert.equal(record.faults, 3);
  assert.equal(record.calibrated, true);
  assert.equal(record.armed, true);
});

test("negative values keep their sign through the double-dash encoding", () => {
  const fixture = FIXTURE_CASES.find((c) => c.id === "negative_values");
  const { record } = M.parsePacket(fixture.packet, null);
  assert.equal(record.altitude, -12.5);
  assert.equal(record.temperature, -5.0);
  assert.equal(record.yaw, -180.0);
  assert.equal(record.az, -9.81);
});

test("a packet from another team is rejected when a team is expected", () => {
  const canonical = FIXTURE_CASES.find((c) => c.id === "canonical");
  assert.ok(M.parsePacket(canonical.packet, "CAN-Team-07").record);
  const other = M.parsePacket(canonical.packet, "CAN-Team-42");
  assert.equal(other.record, undefined);
  assert.match(other.error, /team/);
});

/* -------------------------------------------------------------- validator */

function record(n, timestampMs = n * 1000) {
  return { team_id: "CAN-Team-07", packet_number: n, timestamp_ms: timestampMs,
           gps_lat: null, gps_lon: null };
}

test("a sequential stream is accepted with nothing missing", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  for (let n = 1; n <= 5; n++) {
    const r = v.check(record(n));
    assert.equal(r.accepted, true);
    assert.equal(r.missing, 0);
    assert.equal(r.duplicate, false);
  }
  assert.equal(v.stats.accepted, 5);
  assert.equal(v.stats.missing, 0);
});

test("a gap is counted as the number of packets that never arrived", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  v.check(record(1));
  const r = v.check(record(5));
  assert.equal(r.missing, 3);
  assert.equal(v.stats.missing, 3);
});

test("a repeated packet number is flagged as a duplicate", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  v.check(record(1));
  v.check(record(2));
  const r = v.check(record(2));
  assert.equal(r.duplicate, true);
  assert.equal(v.stats.duplicates, 1);
});

test("an earlier packet number is flagged as out of order", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  v.check(record(5));
  const r = v.check(record(3));
  assert.equal(r.outOfOrder, true);
  assert.equal(v.stats.outOfOrder, 1);
});

test("a foreign team id is rejected and counted", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  const r = v.check({ ...record(1), team_id: "CAN-Team-99" });
  assert.equal(r.accepted, false);
  assert.equal(v.stats.wrongTeam, 1);
  assert.equal(v.stats.rejected, 1);
});

test("a backwards timestamp is noted without discarding the packet", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  v.check(record(1, 5000));
  const r = v.check(record(2, 1000));
  assert.equal(r.accepted, true);
  assert.equal(v.stats.tsReg, 1);
});

test("duplicate detection is bounded, so a long session cannot grow without limit", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  v.seenWindow = 100;
  for (let n = 1; n <= 500; n++) v.check(record(n));
  assert.ok(v._seen.size <= 100, `seen set grew to ${v._seen.size}`);
  assert.ok(v._seenOrder.length <= 100);
  // Recent numbers are still remembered.
  assert.equal(v.check(record(500)).duplicate, true);
});

test("a vehicle reboot is recognised, not read as a stream of duplicates", () => {
  // The watchdog is designed to reboot the vehicle. Its counter then restarts at P-001 and
  // its mission clock at zero; without recognising that, every later packet reads as a
  // duplicate and the loss statistics become meaningless.
  const v = new M.StreamValidator("CAN-Team-07");
  for (let n = 1; n <= 20; n++) v.check(record(n));

  const restart = v.check(record(1, 0));
  assert.equal(restart.restarted, true);
  assert.equal(restart.duplicate, false);
  assert.equal(restart.outOfOrder, false);
  assert.equal(v.stats.restarts, 1);

  for (let n = 2; n <= 10; n++) {
    const after = v.check(record(n));
    assert.equal(after.duplicate, false, `P-${n} after restart`);
    assert.equal(after.outOfOrder, false, `P-${n} after restart`);
  }
  assert.equal(v.stats.duplicates, 0);
});

test("a corrupted P-001 without a clock regression is still a duplicate", () => {
  // Both signals are required, or one bad packet would reset the console's whole view.
  const v = new M.StreamValidator("CAN-Team-07");
  for (let n = 1; n <= 10; n++) v.check(record(n));
  const spurious = v.check(record(1, 99000));
  assert.equal(spurious.restarted, false);
  assert.equal(spurious.duplicate, true);
  assert.equal(v.stats.restarts, 0);
});

test("the first packet of a session is never a restart", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  assert.equal(v.check(record(1, 0)).restarted, false);
  assert.equal(v.stats.restarts, 0);
});

test("an implausible GPS fix is flagged but the packet is kept", () => {
  const v = new M.StreamValidator("CAN-Team-07");
  const r = v.check({ ...record(1), gps_lat: 999, gps_lon: 999 });
  assert.equal(r.accepted, true);
  assert.equal(v.stats.gpsRej, 1);
  assert.match(r.notes.join(" "), /GPS/);
});

/* ------------------------------------------------------------ link health */

test("a paced stream reads back its own rate", () => {
  const h = new M.LinkHealth();
  const now = performance.now() / 1000;
  h._arrivals = [now - 4, now - 3, now - 2, now - 1, now]; // 1 Hz over 4 s
  assert.ok(Math.abs(h.rate - 1.0) < 0.01, `rate was ${h.rate}`);
});

test("a same-instant burst cannot inflate the rate", () => {
  // The defect this guards: an EWMA of instantaneous 1/dt read 249 Hz on a 2 Hz link
  // when a duplicate arrived in the same millisecond as its original.
  const h = new M.LinkHealth();
  const now = performance.now() / 1000;
  h._arrivals = [now - 1, now, now, now, now];
  assert.ok(h.rate < 10, `burst inflated the rate to ${h.rate}`);
});

test("the rate falls to zero when the link drops instead of freezing", () => {
  const h = new M.LinkHealth();
  const now = performance.now() / 1000;
  h._arrivals = [now - 60, now - 59]; // older than the averaging window
  assert.equal(h.rate, 0);
});

test("loss percentage is missing over expected", () => {
  const h = new M.LinkHealth();
  h.packetsOk = 97;
  h.missing = 3;
  assert.ok(Math.abs(h.lossPct - 3.0) < 1e-9);
});

test("loss percentage is zero before any packet arrives", () => {
  assert.equal(new M.LinkHealth().lossPct, 0);
});

test("connection is false until a frame arrives", () => {
  const h = new M.LinkHealth();
  assert.equal(h.connected, false);
  assert.equal(h.age, null);
  h.onFrame("packet");
  assert.equal(h.connected, true);
  assert.ok(h.age < 1);
});

test("frame kinds are counted separately", () => {
  const h = new M.LinkHealth();
  h.onFrame("packet");
  h.onFrame("status");
  h.onFrame("crc");
  assert.equal(h.frames, 3);
  assert.equal(h.statusFrames, 1);
  assert.equal(h.crcErrors, 1);
});

test("the rate window matches the Python ground station", () => {
  assert.equal(M.RATE_WINDOW_S, 5);
});

test("the console distinguishes a magnetic yaw from a relative one", () => {
  const base = "CAN-Team-07; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; " +
    "Ro-1.0; Pi-2.0; Ya-30.0; AX-0.10; AY-0.20; AZ-9.80;";

  const magnetic = M.parsePacket(base + " YR-M;", "CAN-Team-07");
  assert.equal(magnetic.error, undefined);
  assert.equal(magnetic.record.yaw_reference, "magnetic");
  // Yaw runs anticlockwise about the vehicle's up axis; a compass bearing runs
  // clockwise, so the console converts once, here, and only when it may.
  assert.ok(Math.abs(magnetic.record.heading - 330) < 1e-9);

  const relative = M.parsePacket(base + " YR-G;", "CAN-Team-07");
  assert.equal(relative.record.yaw_reference, "gyro");
  assert.equal(relative.record.heading, null);

  // A packet from before the nine-axis upgrade says nothing either way, and the console
  // must not guess on its behalf.
  const silent = M.parsePacket(base, "CAN-Team-07");
  assert.equal(silent.record.yaw_reference, null);
  assert.equal(silent.record.heading, null);

  // The bearing is always inside [0, 360).
  const negative = M.parsePacket(base.replace("Ya-30.0", "Ya--150.0") + " YR-M;",
                                    "CAN-Team-07");
  assert.ok(Math.abs(negative.record.heading - 150) < 1e-9);
});
