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
const SCENARIOS = join(REPO_ROOT, "test-data", "validator-scenarios.tsv");

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
    `${source}\nreturn { crc16ccitt, frameEncode, FrameDecoder, parsePacket, StreamValidator, LinkHealth, RATE_WINDOW_S, parseBridgeStatus, syncWordLabel, SYNC_TEST, SYNC_LAUNCH, unescapeRaw, missionStateView };`
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
  // Counted as an overflow, not a generic resync: a length no frame on this link can have
  // means a corrupted header or a misconfigured sender, and that is worth saying.
  assert.equal(decoder.overflows, 1);
  assert.equal(decoder.resyncs, 0);
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
  assert.equal(record.fault_count, 0);
});

test("all four diagnostic tags are decoded", () => {
  const fixture = FIXTURE_CASES.find((c) => c.id === "diagnostics_only");
  const { record } = M.parsePacket(fixture.packet, null);
  assert.equal(record.mode, "FLIGHT");
  assert.equal(record.fault_count, 3);
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

test("the bridge status line is parsed field by field", () => {
  const line = "#state=RX radio=1 frames=412 dropped=3 rssi=-84 snr=7.5 sync=0xF3";
  const s = M.parseBridgeStatus(line);
  assert.equal(s.radio, true);
  assert.equal(s.frames, 412);
  assert.equal(s.dropped, 3);
  assert.equal(s.rssi, -84);
  assert.equal(s.snr, 7.5);
  assert.equal(s.sync, 0xF3);

  // A negative SNR is the interesting case at the edge of a link, and the minus sign has
  // to survive.
  assert.equal(M.parseBridgeStatus("#state=RX radio=1 snr=-12.5").snr, -12.5);
  // A radio the bridge cannot talk to is false, not missing.
  assert.equal(M.parseBridgeStatus("#state=RX radio=0").radio, false);
});

test("a field the bridge did not send is absent rather than guessed", () => {
  // The caller keeps the previous value for anything absent, which only works if absence
  // is distinguishable from a value.
  const s = M.parseBridgeStatus("#state=RX radio=1");
  assert.equal("rssi" in s, false);
  assert.equal("sync" in s, false);
  assert.equal("dropped" in s, false);
  assert.deepEqual(M.parseBridgeStatus("#bridge=host-stub"), {});
});

test("the sync word is read from the bridge, not assumed", () => {
  // The console printed the test word from a literal in its own markup. Both words are
  // one reflash apart, so the display has to come from what the bridge reports.
  assert.equal(M.parseBridgeStatus("#bridge=online radio=1 sync=0xA5").sync, M.SYNC_LAUNCH);
  assert.equal(M.parseBridgeStatus("#state=RX radio=1 sync=0xf3").sync, M.SYNC_TEST);

  assert.equal(M.syncWordLabel(M.SYNC_TEST), "TEST · 0xF3");
  assert.equal(M.syncWordLabel(M.SYNC_LAUNCH), "LAUNCH · 0xA5");
  // Neither rulebook word: reported as itself rather than labelled as one of them.
  assert.equal(M.syncWordLabel(0x12), "UNKNOWN · 0x12");
  // Nothing reported yet says exactly that.
  assert.equal(M.syncWordLabel(null), "—");
});

/* The validator's shared scenarios. The Python ground station runs the same file through
   its own StreamValidator; two hand-ported implementations agreeing by convention is what
   this replaces. */
function loadScenarios() {
  const text = readFileSync(SCENARIOS, "utf8");
  const scenarios = [];
  for (const rawLine of text.split("\n")) {
    const line = rawLine.replace(/\r$/, "");
    if (!line.trim() || line.startsWith("#")) continue;
    const parts = line.split("\t");
    assert.ok(parts.length >= 4, `malformed scenario line: ${line}`);
    const [name, team, expect] = parts;
    const packet = parts.slice(3).join("\t");
    if (!scenarios.length || scenarios[scenarios.length - 1].name !== name) {
      scenarios.push({ name, team, steps: [] });
    }
    scenarios[scenarios.length - 1].steps.push({ expect, packet });
  }
  return scenarios;
}

const SCENARIO_CASES = loadScenarios();

test("the validator scenario file is present and complete", () => {
  assert.ok(SCENARIO_CASES.length >= 10);
  for (const s of SCENARIO_CASES) assert.ok(s.steps.length > 0, `${s.name} has no steps`);
});

test("every validator scenario matches its recorded verdicts", () => {
  for (const scenario of SCENARIO_CASES) {
    const validator = new M.StreamValidator(scenario.team === "-" ? null : scenario.team);
    scenario.steps.forEach((step, index) => {
      const tokens = new Set(step.expect.split(","));
      let missing = 0;
      for (const token of [...tokens]) {
        if (token.startsWith("missing=")) {
          missing = Number(token.slice("missing=".length));
          tokens.delete(token);
          tokens.add("missing");
        }
      }

      const parsed = M.parsePacket(step.packet, null);
      const where = `${scenario.name}[${index}] expect ${step.expect}`;
      assert.equal(parsed.error, undefined, `${where}: fixture packet must parse`);

      const beforeTs = validator.stats.tsReg;
      const beforeGps = validator.stats.gpsRej;
      const r = validator.check(parsed.record);

      assert.equal(r.accepted, !tokens.has("wrongteam"), where);
      assert.equal(r.duplicate, tokens.has("dup"), where);
      assert.equal(r.outOfOrder, tokens.has("ooo"), where);
      assert.equal(r.restarted, tokens.has("restart"), where);
      assert.equal(r.missing, missing, where);
      assert.equal(validator.stats.tsReg - beforeTs, tokens.has("tsreg") ? 1 : 0, where);
      assert.equal(validator.stats.gpsRej - beforeGps, tokens.has("gpsrej") ? 1 : 0, where);
    });
  }
});

/* The raw-log escaping, held to the same fixture file the Python logger reads. The console
   replays raw logs, so its unescaper and logger.py's have to agree byte for byte. */
const ESCAPES = join(REPO_ROOT, "test-data", "raw-log-escapes.tsv");

function loadEscapeFixtures() {
  const text = readFileSync(ESCAPES, "utf8");
  const rows = [];
  for (const rawLine of text.split("\n")) {
    const line = rawLine.replace(/\r$/, "");
    if (!line.trim() || line.startsWith("#")) continue;
    const parts = line.split("\t");
    assert.equal(parts.length, 3, `malformed escape fixture: ${line}`);
    const [name, escaped, plainHex] = parts;
    const bytes = Uint8Array.from(plainHex.match(/../g) ?? [], b => parseInt(b, 16));
    rows.push({ name, escaped, plain: new TextDecoder().decode(bytes) });
  }
  return rows;
}

const ESCAPE_CASES = loadEscapeFixtures();

test("the raw-log escape fixture is present and complete", () => {
  assert.ok(ESCAPE_CASES.length >= 12);
  const names = new Set(ESCAPE_CASES.map(c => c.name));
  assert.ok(names.has("literal_backslash_then_t"));
  assert.ok(names.has("every_rule_at_once"));
});

test("every raw-log fixture unescapes back to the original", () => {
  for (const c of ESCAPE_CASES) {
    assert.equal(M.unescapeRaw(c.escaped), c.plain, `case ${c.name}`);
  }
});

test("a literal backslash is not read as the escape that follows it", () => {
  // The case the scheme turns on. A backslash followed by the word "there" is not a tab;
  // an unescaper one character out of step reads it as one, inventing a payload the
  // vehicle never sent while trying to recover one.
  assert.equal(M.unescapeRaw("a\\\\tb"), "a\\tb");
  assert.equal(M.unescapeRaw("a\\tb"), "a\tb");
  assert.equal(M.unescapeRaw("a\\\\x41"), "a\\x41");
  assert.equal(M.unescapeRaw("a\\x41"), "aA");
});

test("a malformed escape is left exactly as it was found", () => {
  // A truncated or unknown escape is corruption in a forensic record. Passing it through
  // unchanged keeps the evidence; guessing at it would not.
  assert.equal(M.unescapeRaw("trailing\\"), "trailing\\");
  assert.equal(M.unescapeRaw("\\q"), "\\q");
  assert.equal(M.unescapeRaw("\\xZZ"), "\\xZZ");
  assert.equal(M.unescapeRaw("\\x4"), "\\x4");
});

/* The framing fixtures, shared with framing.cpp and transport.py. */
const FRAMING = join(REPO_ROOT, "test-data", "framing-cases.tsv");

function hexToBytes(hex) {
  return Uint8Array.from(hex.match(/../g) ?? [], b => parseInt(b, 16));
}

function loadFramingCases() {
  const cases = [];
  for (const rawLine of readFileSync(FRAMING, "utf8").split("\n")) {
    const line = rawLine.replace(/\r$/, "");
    if (!line.trim() || line.startsWith("#")) continue;
    const [name, streamHex, events, counters] = line.split("\t");
    const expected = events
      ? events.split(",").map(item => {
          const [kind, payloadHex] = item.split(":");
          return { kind, payload: new TextDecoder().decode(hexToBytes(payloadHex ?? "")) };
        })
      : [];
    const totals = Object.fromEntries(
      counters.split(";").map(part => { const [k, v] = part.split("="); return [k, Number(v)]; }));
    cases.push({ name, stream: hexToBytes(streamHex), expected, totals });
  }
  return cases;
}

const FRAMING_CASES = loadFramingCases();

test("the framing fixture file is present and complete", () => {
  assert.ok(FRAMING_CASES.length >= 12);
  assert.ok(FRAMING_CASES.some(c => c.name === "oversized_length_is_an_overflow"));
});

test("every framing case decodes to its recorded events and counters", () => {
  for (const c of FRAMING_CASES) {
    const decoder = new M.FrameDecoder();
    const got = [...decoder.feed(c.stream)].map(f => ({
      kind: f.crcOk ? "ok" : "crc", payload: f.payload,
    }));
    assert.deepEqual(got, c.expected, c.name);
    assert.equal(decoder.framesOk, c.totals.frames_ok, `${c.name} frames_ok`);
    assert.equal(decoder.crcErrors, c.totals.crc_errors, `${c.name} crc_errors`);
    assert.equal(decoder.resyncs, c.totals.resyncs, `${c.name} resyncs`);
    assert.equal(decoder.overflows, c.totals.overflows, `${c.name} overflows`);
  }
});

test("a framing stream split at every byte decodes identically", () => {
  // A serial port splits wherever it likes; the decoder is incremental for that reason.
  for (const c of FRAMING_CASES) {
    const decoder = new M.FrameDecoder();
    const got = [];
    for (const byte of c.stream) {
      for (const f of decoder.feed([byte])) {
        got.push({ kind: f.crcOk ? "ok" : "crc", payload: f.payload });
      }
    }
    assert.deepEqual(got, c.expected, c.name);
    assert.equal(decoder.overflows, c.totals.overflows, `${c.name} overflows`);
  }
});

/* Every element the console reaches for exists in the console.

   $("#typo") returns null, and the next property access on it throws. In this page that
   would stop the render loop mid-flight, on a display somebody is watching a vehicle
   through. The portable core is DOM-free by construction and cannot make this mistake; the
   half below the marker can, and nothing else checks it. */
test("every id the console selects is declared in the console", () => {
  const html = readFileSync(CONSOLE_HTML, "utf8");
  const declared = new Set([...html.matchAll(/id="([A-Za-z0-9_-]+)"/g)].map(m => m[1]));
  const selected = new Set([
    ...[...html.matchAll(/\$\("#([A-Za-z0-9_-]+)"\)/g)].map(m => m[1]),
    ...[...html.matchAll(/getElementById\("([A-Za-z0-9_-]+)"\)/g)].map(m => m[1]),
    ...[...html.matchAll(/querySelector\("#([A-Za-z0-9_-]+)"\)/g)].map(m => m[1]),
  ]);
  assert.ok(selected.size > 40, "the console should select many elements");
  const missing = [...selected].filter(id => !declared.has(id)).sort();
  assert.deepEqual(missing, [], `selected but never declared: ${missing.join(", ")}`);
});

test("every SVG icon the console uses is defined in the console", () => {
  // Icons are swapped by name at runtime -- setStat() takes an id string and builds a
  // <use href="#..."> from it -- so a renamed symbol fails silently as a blank space
  // rather than as an error.
  const html = readFileSync(CONSOLE_HTML, "utf8");
  const defined = new Set([...html.matchAll(/<symbol id="(i-[A-Za-z0-9_-]+)"/g)].map(m => m[1]));
  const referenced = new Set([
    ...[...html.matchAll(/href="#(i-[A-Za-z0-9_-]+)"/g)].map(m => m[1]),
    ...[...html.matchAll(/"(i-[A-Za-z0-9_-]+)"/g)].map(m => m[1]),
  ]);
  assert.ok(defined.size > 10, "the console should define many icons");
  const undefinedIcons = [...referenced].filter(id => !defined.has(id)).sort();
  assert.deepEqual(undefinedIcons, [],
    `icon referenced but never defined: ${undefinedIcons.join(", ")}`);
});

test("an absent MODE tag is shown as unreported, not as READY", () => {
  // The one that matters: a vehicle can be in FLIGHT or FAULT and have its diagnostic
  // MODE tag dropped. Naming a nominal state the vehicle never claimed is worse than
  // naming none, and the phase ladder must not light a step either.
  for (const absent of [null, undefined, ""]) {
    const view = M.missionStateView(absent);
    assert.equal(view.name, "—");
    assert.equal(view.phase, null);
    assert.match(view.live, /not reported/);
    assert.notEqual(view.name, "READY");
  }
});

test("a MODE tag the vehicle did send is shown as sent", () => {
  const view = M.missionStateView("SELF_TEST");
  assert.equal(view.name, "SELF TEST");
  assert.equal(view.phase, "SELF_TEST");
  assert.equal(view.live, "Mission state self test");
});

test("every mission state the firmware names has a console colour", () => {
  // health.cpp is the single place the firmware turns a MissionState into the string that
  // travels in the MODE tag. A state added there and not here would fall back to the INIT
  // colour silently -- a new state rendered as the oldest one.
  const cpp = readFileSync(
    join(REPO_ROOT, "firmware", "flight-computer", "src", "health.cpp"), "utf8");
  const names = [...cpp.matchAll(/return "([A-Z_]+)";/g)].map(m => m[1]);
  assert.ok(names.length >= 7, `expected the firmware to name at least 7 states, saw ${names.length}`);
  const html = readFileSync(CONSOLE_HTML, "utf8");
  const block = html.slice(html.indexOf("const STATE_COLOR"));
  for (const name of names) {
    assert.ok(new RegExp("^\\s*" + name + ":", "m").test(block),
              `firmware state ${name} has no entry in STATE_COLOR`);
  }
});

test("the presentation layer does not substitute a state for an absent one", () => {
  // missionStateView is only worth having if the renderer goes through it. This is the
  // structural half of the test above: the chip must not reintroduce a default by writing
  // `latest.mode || "READY"` a second time.
  const html = readFileSync(CONSOLE_HTML, "utf8");
  const below = html.slice(html.indexOf("// PORTABLE-CORE:END"));
  assert.ok(below.includes("missionStateView(st)"),
            "the mission-state chip must render through missionStateView");
  assert.ok(!/\.mode\s*\|\|/.test(below),
            "an absent MODE tag must not fall back to a state the vehicle never claimed");
});
