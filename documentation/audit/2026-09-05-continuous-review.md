# Continuous Review — 2026-09-05

A single uninterrupted review pass over the whole repository, run as a loop: find something
wrong, fix it, prove the fix, prevent the class of defect from returning, commit, repeat.

**Reviewer:** automated review with verification at every step
**Scope:** every source file, every document, every fixture, the CI workflow, and every
command the documentation tells a reader to run
**Baseline:** `e87d480`, the last commit of cycle 33 before this pass

**Verdict:** ✅ **Pass. Seventeen findings, all fixed and all covered. Three are defects in
flight or ground software that would have produced wrong data; the rest are documents that
had stopped describing the software, or guarantees nothing was holding.**

---

## What this pass was looking for

The [2026-09-04 audit](2026-09-04-repository-audit.md) verified the software against itself.
This pass started from a different question: **what does this repository claim, and is each
claim still true?** A claim is a number in a document, a command in a runbook, a comment
that says a value is never silent, a counter an operator is told to read.

That question found things a code review does not. Four of the seventeen were found by
running a documented command exactly as written. Three were found by asking what reads a
value the code carefully computes. One was found by reading a comment and checking whether
the code still did what it said.

---

## Findings

Numbering continues the software audit: the previous pass ended at F-42.

| # | Finding | Severity | Status |
|---|---|---|---|
| **F-43** | Every documented test count was stale — the README badge advertised 1351 C++ assertions against an actual 4222, the test plan described 41 flight-core suites where the source called 57 — and nothing checked any of them, while `check_doc_claims.py` was already checking pin numbers for exactly this reason | Medium | ✅ Fixed |
| **F-44** | The web console displayed `TEST · 0xF3` as its sync word from a literal in its own markup. The bridge never reported which word it had configured, so reflashing both ends onto the launch word `0xA5` — the rehearsal the last audit recommends — would have left the console asserting the wrong one on the day it mattered | **High** | ✅ Fixed |
| **F-45** | The README described an `MPU-9250` with an `AK8963` and said yaw "can be referenced to magnetic north". The delivered part is a six-axis `MPU-6500` with no magnetometer, recorded in the hardware documents since it was identified. Its status table also said "no bring-up, no measurement" with 18 of 79 rows already measured | **High** | ✅ Fixed |
| **F-46** | Six design documents and the requirement checklist still described a vehicle that fuses nine axes and can produce an absolute magnetic yaw | Medium | ✅ Fixed |
| **F-47** | `number()` returned the first 63 characters of any value too wide for its buffer — a finite value like `1e300` became a long digit string with no decimal point, a corrupted reading wearing the shape of a reading, kept verbatim in the raw log | Medium | ✅ Fixed |
| **F-48** | The Python and JavaScript stream validators were hand-ports held together by two sets of similarly-named tests. The cross-implementation table said so, listing the guard as "shared test packets" | Medium | ✅ Fixed |
| **F-49** | The runbook's post-flight step replays the raw log. `FileReplayTransport` fed each line to the parser complete with its receipt timestamp, so a real flight log decoded to **`received=0`** — no error, no warning, four hours after a launch with the graphs still to produce | **High** | ✅ Fixed |
| **F-50** | Four documents told the reader to replay `packets.txt`. The repository has never contained it, so the first ground-station command in the quick start ended in `FileNotFoundError` | Medium | ✅ Fixed |
| **F-51** | `live --no-dashboard`, the form the runbook offers for use without a display, printed **nothing at all** when piped: Python block-buffers stdout off a terminal, and twelve seconds of a real run produced no output where a terminal shows five status lines | Medium | ✅ Fixed |
| **F-52** | The status LED cadences and the pad calibration gates — the numbers an operator reads off a vehicle with no radio and no serial cable — were quoted in two documents and defined in one `switch`, with nothing connecting them | Low | ✅ Fixed |
| **F-53** | `RawBlockLog::truncated_records()` counts records cut to fit a 512-byte block, under a comment reading *"Never silent: the flight log is evidence"*. Nothing read it — no health snapshot, no diagnostic, no operator | Medium | ✅ Fixed |
| **F-54** | The Tk dashboard's **Battery (V)** row read a key nothing has ever written, from the bridge — a different Pico with no battery sense. It could only ever display `n/a`, which reads as a link that is not reporting rather than a quantity that is not sent | Low | ✅ Fixed |
| **F-55** | The web console's file replay stripped a raw-log line's timestamp but never undid the escaping, so a payload containing a tab came back carrying a literal backslash and a `t` — and the records a forensic replay exists for are the corrupted ones | Medium | ✅ Fixed |
| **F-56** | Three frame decoders agreed on what a valid frame is and disagreed on what a broken one is: an oversized length field was an `overflow` in C++ and a generic `resync` in the other two | Medium | ✅ Fixed |
| **F-57** | The orientation estimator granted magnetometer confidence whenever a field passed the **magnitude** gate. A field with no horizontal component — a magnetic pole, or a vertical disturbance on the pad — leaves yaw at zero, and the vehicle then transmitted `YR-M`: an absolute magnetic heading of 0° that nothing had measured | **High** | ✅ Fixed |
| **F-58** | The NMEA coordinate parser accepted any of `N`, `S`, `E`, `W` on either axis, so a latitude field carrying `W` parsed as a **southern** latitude — the fix placed on the wrong side of the equator | **High** | ✅ Fixed |
| **F-59** | `Sx1278::poll_receive()` cut a payload longer than the caller's buffer and returned the trimmed length silently. The bridge frames and CRCs a truncated payload like a whole one, so it arrives as a valid frame carrying a malformed packet — a diagnosis pointing at the vehicle when the fault is in the receive path | Low | ✅ Fixed |

---

## The three that would have produced wrong data

**F-57 — a heading nothing measured.** The magnetometer is admitted on field strength.
Recovering a *heading* needs a horizontal component, and a field can have the first without
the second. `seed()` handled that correctly and silently; its caller then set confidence to
the threshold because the magnitude gate had passed, and the next packet declared `YR-M`.
The vehicle's own rule is that it never claims an absolute heading it has not earned. This
was the one path where that did not hold.

**F-58 — a fix on the wrong side of the equator.** `parse_coordinate()` serves both
coordinate fields and was never told which one it was parsing. A latitude marked `W` came
back negative. The checksum catches most corruption; this is what is left when it does not.

**F-49 — a flight log that replayed as nothing.** Not wrong data: no data, reported as a
successful run of zero packets, in the procedure a team follows after a launch with four
hours to produce three graphs.

None of the three is reachable on the vehicle as it stands today: the delivered IMU has no
magnetometer, so F-57 is dormant until a nine-axis part is fitted. F-58 and F-49 are live.

---

## The shape that kept recurring

Four findings are the same defect wearing different clothes: **a value cut to fit, and
nothing said**.

| Where | What was cut | Now |
|---|---|---|
| `number()` | a value too wide for a 64-byte buffer | empty field, rejected downstream (F-47) |
| `RawBlockLog` | a record too long for a 512-byte block | counted and reported at Gate 6.6 (F-53) |
| `Sx1278::poll_receive()` | a payload longer than the caller's buffer | counted (F-59) |
| the bridge status line | 121 characters into a 128-byte buffer | 160-byte buffer, arithmetic written down |

And two more are its mirror image — **a display with nowhere to get its value from**: the
console's sync word (F-44) and the dashboard's battery row (F-54).

---

## What the fixes are held to

Every finding was closed with a test that fails against the previous code. Where the defect
was a document, it was closed with a check in `tools/check_doc_claims.py`, which runs inside
`tools/build_host.sh` and therefore inside CI.

Counts below are what the suites actually reported, then and now — not what the documents
said, which was the subject of F-43.

| | At `e87d480` | Now |
|---|---:|---:|
| Documented claims checked | 66 | **159** |
| C++ assertions | 4222 | **4260** |
| Python tests | 131 | **152** |
| Node tests | 37 | **49** |
| Shared cross-implementation fixtures | 1 | **4** |

The four fixtures now cover every pair of implementations the cross-implementation table
names: packet format, validation semantics, raw-log escaping, and framing. What remains in
that table is single-definition code, where drift is not possible.

Nine kinds of new check exist because a document was wrong in a way no test could see: total
test counts, per-suite counts, bring-up progress, the delivered IMU's identity, the CI job
count, LED cadences, calibration gates, the sample mission's field order, and that every
document naming a file names one that exists.

---

## What was read and found correct

Recording this because "no defect found" is a result, and because the next reviewer should
know where this pass has already been.

- **BMP280 compensation** — reproduces the Bosch 64-bit reference path exactly, and the
  datasheet vector test was already there.
- **The Mahony estimator** — quaternion integration, the body-to-level rotation, the
  magnetometer reference construction that keeps a disturbance out of roll and pitch, the
  bias integrator's sign and clamp, the compass-bearing conversion. Only the seeding path
  was wrong.
- **The startup calibrator** — including its separation of "not shaking" from "not turning",
  which is the distinction a constant pad rotation would otherwise slip through.
- **The magnetometer bounding-box calibration** — the zero-initialised bounds are never
  used, because the first accepted sample seeds both ends.
- **The mission state machine** — launch confirmation, the arming lockout, the landing
  discriminator that does not trust a 1 g reading under a parachute.
- **The six-axis degrade path** — `mag_unavailable` is a warning, self-test gates only on
  the IMU and barometer, and the runtime calibrator early-returns. The vehicle flies
  correctly on the part it actually has.
- **The link-health implementations** — Python and JavaScript agree; the only divergence is
  Python rounding for display.
- **The CI workflow** — every gate blocks, CTest covers the same five C++ suites as the host
  script, and the toolchain is pinned deliberately.
- **The web console's DOM half**, exercised in a browser against every packet shape the
  parser accepts, including all four diagnostic tags, both yaw references and an unknown
  optional field. No exception, no wrong field.

---

## Open, and deliberately not closed here

| Item | Why it is open |
|---|---|
| `poll_receive()` clears the IRQ flags before reading the FIFO, where Semtech's examples read first | A race whose window is microseconds against a 1 Hz packet rate, in the one path on this radio hardware has never exercised. Reordering a register sequence on a part that passed its bench gate with the current order is a change to make with the radio in front of you |
| Transmitting the battery voltage as `BAT-x.xx` | About nine bytes against a 255-byte budget whose measured worst case is 206 — affordable, and a change to the packet contract. The protocol document requires a documented consumer and a bandwidth assessment first, and the decision is the team's |
| Everything the [bring-up record](../testing/bring-up-record.md) still has blank | 61 of 79 rows. Needs hardware, and Gate 2 — power — has not been started at all |

---

## Audit trail

| Step | Command | Result |
|---|---|---|
| Host build and tests | `bash tools/build_host.sh` | All pass, zero warnings |
| Strict warning set | `CXXFLAGS="… -Werror" bash tools/build_host.sh` | Clean |
| Firmware syntax | `bash tools/check_pico_syntax.sh` | 11 / 11 `OK` |
| Documented claims | `python tools/check_doc_claims.py` | 159 / 159 |
| Documented commands | run as written, from the directory each document names | All pass |
| Internal links | every relative Markdown link resolved | 0 broken |
| Web console | driven in a browser, demo and injected packets | No console errors |

Each fix was additionally verified by reverting the change and confirming its test fails.
