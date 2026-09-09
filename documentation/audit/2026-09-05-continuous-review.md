# Continuous Review — 2026-09-05

A single uninterrupted review pass over the whole repository, run as a loop: find something
wrong, fix it, prove the fix, prevent the class of defect from returning, commit, repeat.

**Reviewer:** automated review with verification at every step
**Scope:** every source file, every document, every fixture, the CI workflow, and every
command the documentation tells a reader to run
**Baseline:** `e87d480`, the last commit of cycle 33 before this pass

**Verdict:** ✅ **Pass. Thirty-three findings, all fixed and all covered. Four would have cost
a mission or a diagnosis: three produce wrong data, and one leaves a vehicle refusing to fly
without saying why. The rest are documents that had stopped describing the software, or
guarantees nothing was holding.**

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

Numbering continues the software audit: the previous pass ended at F-42. F-43 to F-59 were
found by reading; [F-60 to F-75](#second-wave--f-60-to-f-75) by comparing one
machine-readable thing against another.

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

## Second wave — F-60 to F-75

The findings above were reached by reading. Once the obvious classes were closed, the way to
find more was to stop reading and start **comparing one machine-readable thing against
another**: the snapshot against the display, one parser's record against the other's, a
document's numbers against the table beneath them. Every finding below came out of a
comparison a script can repeat.

| # | Finding | Severity | Status |
|---|---|---|---|
| **F-60** | Thirty-four documents, a table of contents in most, cross-references throughout — and nothing checked that any link or heading anchor still pointed anywhere | Low | ✅ Fixed |
| **F-61** | `TEL-018`, `TEL-019` and `TEL-020` — three of the rulebook's mandatory telemetry fields — cited `test_mpu_scaling` as their compliance evidence. That test was renamed `test_imu_scaling` when the IMU changed, and a requirement pointing at nothing reads exactly like a requirement that is covered | Medium | ✅ Fixed |
| **F-62** | `TEL-015` (roll transmitted) was `Complete`; `SEN-008` (roll generated and transmitted) was `Not Started`. One page, one fact, two statuses — and the whole `SEN-001`–`SEN-009` block still read "integration - TBD" for drivers written, tested, and since read on the bench | Medium | ✅ Fixed |
| **F-63** | `PWR-004` and `GEN-008` were `Not Started` for behaviour that is implemented, tested and, in the radio's case, already transmitting on a bench | Low | ✅ Fixed |
| **F-64** | `FrameDecoder`'s resync and overflow counters were read by nobody — **including the overflow counter added earlier in this same pass**, whose entire justification was that the distinction is worth showing an operator | Medium | ✅ Fixed |
| **F-65** | The Tk dashboard was missing nine of the values the ground station knows, including **`calibrated` and `armed`** — the two flags an operator stands on a pad waiting for — and the accepted and rejected totals every other validation row is a fraction of | Medium | ✅ Fixed |
| **F-66** | The Python parser exposed `fault_count`; the JavaScript one exposed `faults`. Reading the wrong name raises in Python and yields `undefined` in JavaScript, which the console renders as an em dash: a fault count of *none reported* where the vehicle said three | Medium | ✅ Fixed |
| **F-67** | Post-flight step 5 says to compare the onboard SD log against the ground station's CSV. Of their 18 and 23 columns, `packet_number` is the only name they share, and nothing said how to line them up | Medium | ✅ Fixed |
| **F-68** | The quick start's `pip install -r requirements.txt` installs nothing — the file is entirely comments — and the document never named `pyserial`, which its own step 19 requires | Medium | ✅ Fixed |
| **F-69** | The quick start carries a second copy of the GPIO table, the one somebody wires from with the board in front of them, and nothing held it to the firmware | Low | ✅ Fixed |
| **F-70** | Two of the nineteen fault codes the firmware can raise appeared in no document. One of them, `mag_unavailable`, is **standing on this vehicle right now** | Low | ✅ Fixed |
| **F-71** | `validate_config()` names which of its thirty rules refused a configuration. The controller replaced that with `"configuration invalid"`, then latched a critical fault — leaving an operator a vehicle that will not fly and thirty candidates for why | **High** | ✅ Fixed |
| **F-72** | The bring-up diagnostic's second I2C scan is headed *"AK8963 at `0x0C` **SHOULD** now appear"* — two lines after the same program prints *"SIX axes: there is no magnetometer in this package at all"*. An operator with the board in front of them goes hunting a wiring fault that does not exist | Medium | ✅ Fixed |
| **F-73** | Both hardware documents named the delivered six-axis part in one section and asserted a *"genuine nine-axis part"* in another. **Both passed the rule written earlier in this pass to prevent exactly that**, because it asked whether the file mentions the delivered part rather than whether a paragraph contradicts itself | Medium | ✅ Fixed |
| **F-74** | The test plan's **first instruction** described `build_host.sh` as running four suites. It runs eight — the LoRa driver, the microSD driver, the web console and the documented-claims check were all absent from the list a reader consults to find out what is covered | Low | ✅ Fixed |
| **F-75** | `.gitattributes` requires shell scripts to be LF **in the working tree**, because CI runs them on Linux where a stray CR after the shebang is a `bad interpreter` error. `build_host.sh` had 130 CRLF endings, introduced by this pass's own edits. Git normalised them into the index, so the committed content was always correct and the only symptom was a warning on every commit | Low | ✅ Fixed |

### What the second wave says about the first

Three of these are the same shape as F-53 and F-59 — a value computed and shown to nobody —
and one of them was **introduced by this pass**. Fixing four instances by hand did not stop
the fifth; a script comparing the whole snapshot against the whole display did, and found
eight more in the same pass. That is the difference between fixing instances and closing the
way in.

Two others, F-61 and F-66, are drift the shared fixtures were built to prevent and cannot
see: fixtures hold implementations to one definition of *behaviour*, and both of these were
disagreements about a **name**. Names now have their own checks.

And F-73 is the one worth reading twice, because **a check written in this pass failed to
catch what it was written for**. It asked whether a document mentions the delivered part
anywhere, which two documents did while contradicting it three screens away. Re-examining
the other checks written the same way found the wiring gate would have passed a table whose
pins and signal names had been shuffled against each other — every pin and every name still
present, none of them together. Where a claim is about two things *belonging together*, the
check has to be scoped to the row or the paragraph, and that reasoning is now written at the
top of `check_doc_claims.py` where the next person will meet it.

---

## Third wave — F-76 to F-85

The pass resumed after being stopped, with the working copy green and every earlier gate
passing. The findings below came from the same comparison as the second wave: hold two
representations of one fact side by side and see which one is lying.

| # | Finding | Severity | Status |
|---|---|---|---|
| **F-76** | `MODE` is an optional diagnostic tag, and `parsePacket` correctly keeps its absence as `null` — then the console's state chip wrote `latest.mode \|\| "READY"`. A packet arriving without a `MODE` tag displayed **READY**, and the phase ladder lit the READY step to agree with it. A vehicle in `FLIGHT` or `FAULT` whose tag was dropped would show a calm nominal word | **High** | ✅ Fixed |
| **F-77** | The audit trail in this document recorded `159 / 159` documented claims. The answer had been 199 for hours. A document about keeping documents honest had gone stale about itself, because nothing checked the row that quotes this script's own output | Low | ✅ Fixed |
| **F-78** | `TelemetryValidity::mandatory_valid()` decides whether a record may be transmitted at all — `format_packet()` refuses one it rejects — and no test named it. It is a nine-term AND over a struct of nine flags, with nothing holding the two together: a tenth flag added to the header and forgotten in the function would let a reading the vehicle never took travel as though it had | Medium | ✅ Fixed |
| **F-79** | The hardware reference tables still said `Value on the delivered board - TBD` for the IMU's `WHO_AM_I`, `AD0 wiring and available address - TBD` for its I2C address, and `SDO wiring - TBD` for the barometer's — all three measured on the bench the day before, and recorded nine lines away in the same document. These are the rows an electrical design is drawn from | Medium | ✅ Fixed |
| **F-80** | `electrical-architecture.md` had never learned the part identity at all: `MPU-9250` in the bill of materials, `exact board documentation TBD`, and a Gate 2 blocker demanding the team identify a variant that had already been identified by register read | Medium | ✅ Fixed |
| **F-81** | The same tables asserted a magnetometer configuration — 16-bit continuous mode 2 at 100 Hz, per-axis ASA applied — in the *Breakout-board status* column, the one column that is specifically about the delivered board. The delivered board has no AK8963 | Low | ✅ Fixed |
| **F-82** | The bench settled the barometer variant on 2026-09-05 — chip ID `0x58`, a BMP280 — and four documents went on asking for it. `hardware.md` called it `TBD - blocking`, `pre-procurement-design-status.md` called it `Blocking`, `wiring.md` left the checkbox unticked, and `photos/README.md` still asked for a die photograph the register had made unnecessary | Medium | ✅ Fixed |
| **F-83** | The same bus scan settled both I2C strap directions — the IMU at `0x68` and the barometer at `0x76`, so AD0 and SDO are both low — and two documents still asked for them, one of them specifying a meter for a question the scan had already answered | Low | ✅ Fixed |
| **F-84** | Two more items the bench closed were still listed as open: the battery polarity, read on a meter at 3.92 V with red positive, and the 1S charger, bought on 2026-09-05. The charger item was genuinely half-open — the absent charge parameters — and now says which half | Low | ✅ Fixed |
| **F-85** | The runbook's post-flight command replayed `logs/raw_packets.tsv` with `--output` left at its default of `logs`, so the station opened the file it was reading for append. The reader kept finding the lines the writer had just written: the replay never ends, the flight's only forensic record fills with re-logged copies of itself, and the disk fills behind it. Reached 69 MB in under two minutes | **High** | ✅ Fixed |

### F-76 in detail

The console already had a test named *a field the bridge did not send is absent rather than
guessed*, written for the bridge status line. The state chip broke the same rule on the
field with the most operational weight on the page, and no test covered it, because the
substitution lived in the renderer rather than in the portable core.

The fix moves the decision into the core as `missionStateView()`, which returns the name to
show, the text to announce, and the phase to light — `null` for all three when the vehicle
did not say. Three gates now hold it:

- an absent `MODE` renders as an em dash and never as a state name;
- the renderer is required to go through `missionStateView`, and the string `.mode ||` is
  forbidden below the portable-core marker, so the default cannot be written a second time;
- every state name `health.cpp` returns must have an entry in the console's `STATE_COLOR`.

The third gate found a live gap on its first run: `health.cpp` returns `UNKNOWN` for a
`MissionState` outside the enum, and the console had no entry for it, so it would have been
painted in the muted `INIT` styling. It now has its own warning colour.

The same audit removed one more coalesce: the mission sample builder wrote `r.mode || ""`.
An empty string is harmless there — it matches no state name — but it is the same habit,
and every consumer of that field compares with `===`, so `null` carries through unchanged.


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

A fifth is the same idea one step further on, and the worst of them: a diagnosis that was
not merely uncollected but **computed, handed over, and discarded**. `validate_config()`
told the controller exactly which rule had refused the configuration, and the controller
wrote `"configuration invalid"` instead (F-71). The vehicle then sat in `FAULT`, unable to
produce a compliant packet, with nothing to say about it.

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
| Documented claims checked | 66 | **215** |
| C++ assertions | 4222 | **4268** |
| Python tests | 131 | **160** |
| Node tests | 37 | **51** |
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

## Where the pass stopped finding things

Recorded because knowing where a review ran out is part of knowing what it covered.

The last several comparisons came back clean, and that is a result rather than a gap:

- **The console's DOM.** Every `$("#id")` resolves, and every icon named at runtime is
  defined. Both are now tests, because the failure mode — a null dereference stopping the
  render loop mid-flight — is silent until it happens.
- **The dashboard's variables.** Every variable written is one it created. Also now a test:
  the failure is a `KeyError` on the first snapshot.
- **The documentation index.** All 21 documents are listed.
- **The configuration surface.** 60 of 78 fields are undocumented on purpose; the runbook
  lists the six an operator should think about, and burying those under seventy internal
  tuning constants would make it worse.
- **The C++ record's field names.** Deliberately not aligned with the receivers': it names
  its units, which is worth more on an embedded target than matching a struct in another
  language that is not a port of it.

What remains unexamined is what cannot be examined here: the Pico HAL has been read but
never executed, and every hardware gate below still needs the bench. That boundary has not
moved in this pass, and no finding above changes it.

## What this pass would do differently

Recorded because the method mattered more than any single finding.

**Reading found the first seventeen. Comparison found the rest, faster.** Once the obvious
classes were closed, reading the same code again returned almost nothing, while a script
comparing the snapshot against the display found eight defects in one run. The transferable
part is not *read everything* but: **find two representations of the same fact and diff
them.** The snapshot against the dashboard, one parser's record against another's, the
`FaultCode` enum against the fault table, the `add_test` calls against the documented count.

**Executing a document beats reading it.** Four findings came from typing a documented
command exactly as written — `packets.txt` did not exist, the raw-log replay returned
`received=0`, the headless monitor printed nothing through a pipe, the install step
installed nothing. None would have been found by reading the code those commands reach,
and none by reading the document either.

**A check can be wrong in the shape of the thing it checks.** F-73 was missed by a rule
written in this same pass to prevent it. The rule asked whether a file mentions a fact;
the defect was a paragraph contradicting it elsewhere in the same file. When a claim is
about two things belonging together, scope the check to where they belong together.

**Reverting the fix to watch the test fail is worth the minute it costs.** Every fix here
was confirmed that way, and one of them — the raw-log replay — proved the test was checking
something the old code already satisfied, which meant writing a sharper one.

**A warning that appears on every run is being ignored, not tolerated.** F-75 printed
`CRLF will be replaced by LF` on every commit of this pass before anyone read it. The habit
worth keeping is not *investigate every warning* — most of the ones here are correct and
expected — but *know which ones are expected, and be able to say why*. Once the remaining
warnings were checked, they turned out to be the `text=auto` Markdown ones, which are
correct by design.


## Audit trail

Final state, re-run from a fresh `git clone` with no build directory:

| Step | Command | Result |
|---|---|---|
| Host build and tests | `bash tools/build_host.sh` | All pass, zero warnings |
| Strict warning set | `CXXFLAGS="… -Werror" bash tools/build_host.sh` | Clean |
| Firmware syntax | `bash tools/check_pico_syntax.sh` | 11 / 11 `OK` |
| Documented claims | `python tools/check_doc_claims.py` | 255 / 255 |
| Documented commands | run as written, from the directory each document names | All pass |
| Internal links | every relative Markdown link resolved | 0 broken |
| Web console | driven in a browser, demo and injected packets | No console errors |
| Fresh clone | `git clone`, then the whole suite and a documented command, with no prior build state | All pass — every fixture and sample this pass added is tracked, and nothing depends on a built artefact |

Each fix was additionally verified by reverting the change and confirming its test fails.
