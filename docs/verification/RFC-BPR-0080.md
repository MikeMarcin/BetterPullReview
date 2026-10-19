# RFC-BPR-0080 hash evaluation

This is the committed report [RFC-BPR-0080](../rfcs/RFC-BPR-0080-hashing-replacement.md)
AC01 asks for. [[BPR-1446]] produced it on macOS arm64. Immutable
RFC_BASE_SHA: `5620a9f15a391dde7750112294f7562f2f64a32f`. The harness is
`tests/hash_evaluation` at the revision each raw file names; how to run it is
in [Build and Test](../development/build-and-test.md#hash-evaluation).
Windows ([[BPR-1448]]) and Linux ([[BPR-1449]]) have not run. Nothing below
claims a result for them.

## Owner answers

| Question | Answer | Date |
|---|---|---|
| Which libraries may core link? | "Core may link only the owner-approved pinned, pure (no I/O, threads, clocks or globals) libraries: glaze, utf8proc, zlib and fmt" (root `AGENTS.md`) | 2026-10-13 |
| Q1: add sha1collisiondetection (MIT) to core's approved libraries? | Accept | 2026-10-13 |
| Q2: add HACL*'s generated SHA-2 C (MIT) to core's approved libraries? | Accept | 2026-10-13 |
| Waive the idle-machine condition for the macOS speed measurements | Accept ("the loaded macOS speed measurements are accepted for the adopt rules") | 2026-10-13 |
| Amendment: the collision requirement is met by detection on raw byte streams plus the same library at every object-ID path, shown by inspection and test | Accept ("A Git-header collision vector is not required") | 2026-10-13 |
| Q3: may the Linux build link the system libcrypto directly? | Not asked: the Linux backend has not been measured | — |

## Decisions

The decisions below were made before the owner's answers. With Q1 and Q2
accepted and the idle-machine condition waived for these macOS rows, both
candidates meet their adopt rules on macOS; adoption is [[BPR-1450]] and
[[BPR-1451]].

| Component | Role | Decision | Rule result on macOS arm64 |
|---|---|---|---|
| sha1collisiondetection | SHA-1 Git identities and verification of fetched bytes | **Eligible pending Q1** | Gate: zero mismatches. Detection: both public pairs detected while plain SHA-1 collides; the repository sweep detected nothing. W1 to W3: 2.55x to 2.96x the current core SHA-1 (rule: at least 1.0x; run 1: 2.06x to 3.03x), and 1.53x to 1.90x the control arm. The digest-using rows and the four refusal points belong to adoption. A Q1 Accept is necessary but not sufficient: the AC03 finding below also needs an amendment or a Git-framed collision vector (the owner accepted that amendment on 2026-10-13). |
| HACL* `Hacl_Hash_SHA2` | Core SHA-256 | **Eligible pending Q2** | Gate: zero mismatches, including `main`'s whole history as SHA-256. W1 to W3: 1.18x to 1.48x the core SHA-256 (rule: at least 0.9x; run 1: 1.32x to 1.71x). The block-level wrapper allocates nothing. |
| CommonCrypto behind a host port | Bulk SHA-256 the host already holds, on macOS | **Throughput condition passes; site decision pending [[BPR-1447]]** | Inputs of 1 MiB or more: 7.11x to 8.54x core (rule: at least 3x on a SHA-extension target; run 1: 7.35x to 9.56x). The site condition (10 ms less window-thread hashing at 10k, or none on the window thread) can be measured only with the port built. |
| CNG `BCrypt*` behind the port | The same, on Windows | **Unmeasured** | Station missing ([[BPR-1448]]). |
| System libcrypto behind the port | The same, on Linux | **Unmeasured** | Station missing ([[BPR-1449]]); Q3 is asked only if this passes. |
| `QCryptographicHash` | Identity and bulk roles | **Rejected, as before** | SHA-256 at 0.52x to 0.71x core over both runs, as the RFC's grounding table found (247 against 433 MB/s). Its SHA-1 is 2.64x to 4.84x core, also as the grounding found, but detects nothing. Its existing non-identity uses are unchanged. |
| First-party SHA-1 hashing whole blocks | The RFC's control arm | **Measured, not adopted** | 1.45x to 1.95x core SHA-1 over both runs, below sha1dc on every workload. [[BPR-1452]] takes it up only on a Q1 Reject. |

## Stations and builds

| Station | Result |
|---|---|
| macOS arm64 | Apple M4 Pro, 14 logical CPUs, `hw.optional.arm.FEAT_SHA1`, `FEAT_SHA256`, `FEAT_SHA512` and `FEAT_SHA3` all 1; macOS 26.6.2 (25G83); Apple clang 21.0.0 (clang-2100.1.1.101); CMake `Release` (`-O3 -DNDEBUG`), tests on, pinned Qt 6.11.2; Git 2.50.1 (Apple Git-155). Measured. |
| macOS x86_64 slice | Correctness only, as the RFC asks: the harness built for `x86_64` (`core-only` preset, `Release`, no Qt arm) and run under Rosetta 2 on the same Mac. Known answers, chunking, the seeded differential, the collision inputs and Git identities in both formats: zero mismatches; collision results identical ([raw](RFC-BPR-0080/macos-x86_64-rosetta-correctness.json)). |
| Windows x64 | Not run: [[BPR-1448]]. |
| Linux x64 | Not run: [[BPR-1449]]. |

**The machine was not idle.** Other agents were building on the same Mac
throughout: the one-minute load average stayed between about 45 and 155 on 14
logical CPUs, so the RFC's idle-machine condition could not be met. Wall-clock
times mostly measured descheduling (a 64 MiB core SHA-1 took 3.7 s of wall time
for 0.41 s of CPU). The throughput tables therefore use each run's **thread CPU
time**, with the thread at user-interactive QoS so it stays on a performance
core, arms interleaved within each run, one warm-up, and the median of 7. The
wall-clock medians are in the raw file. Absolute rates are below the RFC's
grounding table, which used Homebrew clang 23 and best-of-5 runs; the decisions
rest on ratios measured side by side in two runs, and the narrowest margin is
HACL*'s 1.18x against a 0.9x rule. On 2026-10-13 the owner waived the idle condition for these macOS rows and
accepted the loaded measurements for the adopt rules.

## Correctness gate

Zero mismatches for every implementation in every row. Each arm is compared
with the published answer (known answers) or with the current core
implementation (everything else), and the core implementation is in turn
compared with CommonCrypto.

Run at `d9bfa436b` ([raw](RFC-BPR-0080/macos-arm64-correctness.json)). SHA-1
arms: core, first-party-block (the control arm), sha1dc, CommonCrypto, Qt.
SHA-256 arms: core, HACL*, CommonCrypto, Qt.

| Row | Inputs | Cases per arm | Mismatches |
|---|---|---|---|
| Known answers (`kat`) | NIST CAVP byte-oriented `SHA1` and `SHA256` `ShortMsg` (65) and `LongMsg` (64), the 100 `Monte` checkpoints of 1,000 chained digests each, and the four FIPS 180-4 examples per algorithm | 233 per algorithm | 0 |
| Chunking | Lengths 0 to 130, 1,000 and 1 MiB; every two-update split of 200 bytes with an empty update between; 1 MiB + 17 bytes in uneven chunks (1 to 65,537 bytes) | 335 per algorithm, plus 133 core-against-CommonCrypto | 0 |
| Seeded differential (seed 20261013) | 2,304 inputs per algorithm: every length 0 to 300, 2,000 random lengths up to 1 MiB, then 16, 64 and 256 MiB; 1.42 GB per algorithm. Each arm hashes each input in one update and in seeded random chunks | 4,608 per algorithm, plus 2,304 core-against-CommonCrypto | 0 |
| Git blob identities | 55 seeded files, 0 bytes to 256 MiB (308.6 MB), against `git hash-object` in a SHA-1 and a SHA-256 repository | 55 per format | 0 |
| Git trees and commits | The same files in nested folders: `write-tree`, `mktree` from its listing (same identity), and two `commit-tree` commits; then every object of each repository (55 blobs, 21 trees, 2 commits) | 78 per format | 0 |
| This repository, SHA-1 | Every object in the object store (`git cat-file --batch-all-objects --batch`; a superset of every reachable object), `main` at `78b03113f`: 106,013 objects, 2.33 GB (35,293 blobs, 61,707 trees, 9,003 commits, 10 tags) | 106,013 | 0; sha1dc detections 0 |
| This repository, SHA-256 | `main`'s history converted by `fast-export`/`fast-import` into a SHA-256 repository: 93,019 objects, 2.02 GB (33,326 blobs, 52,397 trees, 7,296 commits) | 93,019 | 0 |

The existing `core_sha256` and `core_git_objects` rows test the core
implementations, which this evaluation does not change. The digest-using rows
BPR-1358 named exercise the product's hash; they run against a candidate only
when an adoption card puts it in core.

## Collision inputs

`bench_hash_evaluation collision` over the test files in the pinned
sha1collisiondetection archive (`hash_evaluation_collisions` runs the same as
a row):

| Input | Plain SHA-1 (core) | sha1dc, raw bytes | Hashed as a Git blob |
|---|---|---|---|
| SHAttered (`shattered-1.pdf`, `shattered-2.pdf`, 422,435 bytes each, different contents) | Both `38762cf7f55934b34d179ae6a4c80cadccbb7f0a`: collides | Detected in both; the digest beside it equals plain SHA-1 | Different identities (`ba9aaa14…`, `b621eecc…`); not detected |
| SHA-mbles (`sha-mbles-1.bin`, `sha-mbles-2.bin`, 640 bytes each) | Both `8ac60ba76f1999a1ab70223f225aefdc78d4ddc0`: collides | Detected in both; digest equals plain SHA-1 | Different identities (`5a7c30e9…`, `fe391784…`); not detected |
| `sha1_reducedsha_coll.bin` (128 bytes, a reduced-round collision) | — | Not detected as Git configures sha1dc; detected with reduced-round detection on | — |

The repository sweep above detected nothing: zero sha1dc detections across
all 106,013 objects.

**Finding for AC03.** A Git identity hashes `<type> <size>\0` before the
content, which moves the colliding blocks off their 64-byte boundaries and
changes the chaining value they were built for. So neither public pair
collides as a Git object, and sha1dc rightly detects neither there; Git
itself stores both PDFs as two distinct blobs. AC03 asks that every collision
input be refused at core identity construction, host blob verification,
transport verification and Review Cycle publication. Those four points all
hash Git objects, so the public vectors can exercise refusal only at the
stream and one-shot SHA-1 level. Demonstrating the four object-level points
needs a collision built with a Git object header as its prefix, which no
pinned archive supplies, or an amendment that accepts stream-level refusal
plus proof that every identity path goes through the detecting stream. This
is recorded for [[BPR-1450]]. It does not change what this evaluation
measured, but it means a Q1 Accept alone cannot satisfy the RFC's sha1dc
adopt rule as written: adoption also needs that amendment or such a vector.

## Throughput

Two runs, each the median of 7 after a warm-up, arms interleaved within each
run, in thread CPU time (see "The machine was not idle" above). Run 2 is at
`3f9e45980`, where the candidate libraries are built hardened like the core
([raw](RFC-BPR-0080/macos-arm64-throughput-run2.json)); run 1 is at `40870b777`,
before that change ([raw](RFC-BPR-0080/macos-arm64-throughput-run1.json)). W1 is
64 MiB in one update; W2 is 20,015 separate digests of 400 bytes (BPR-1349's
10k body shape); W3 is 1, 16 and 256 MiB in 64 KiB updates. MB is 10^6 bytes.

Ratios to the current core implementation across W1 to W3, from the medians
and, as a check against load that changed during a run, the median of each
run's own ratio (the arms of one run execute back to back):

| Arm | Rule | Run 2, medians | Run 2, per-run | Run 1, medians | Run 1, per-run |
|---|---|---|---|---|---|
| sha1dc against core SHA-1 | at least 1.0x | 2.55x to 2.96x | 2.58x to 2.87x | 2.06x to 3.03x | 2.06x to 2.80x |
| First-party block SHA-1 (control) | — | 1.56x to 1.76x | 1.43x to 1.72x | 1.45x to 1.95x | 1.45x to 1.95x |
| HACL* against core SHA-256 | at least 0.9x | 1.18x to 1.48x | 1.31x to 1.48x | 1.32x to 1.71x | 1.33x to 1.77x |
| CommonCrypto SHA-256, 1 MiB or more | at least 3x | 7.11x to 8.54x | 7.10x to 7.83x | 7.35x to 9.56x | 7.39x to 9.07x |
| `QCryptographicHash` SHA-256 | — | 0.52x to 0.63x | 0.55x to 0.62x | 0.56x to 0.71x | 0.56x to 0.70x |

Wall-clock ratios are in the raw files and are not used: a wall-clock run here
mostly measured how long the thread waited for a core, which differs from arm
to arm within a run (for example HACL* at W3 1 MiB in run 1 is 0.77x by
wall-clock medians, 1.40x by thread CPU time and 1.03x by the per-run wall
ratio).

Run 2 in full: median MB/s, ratio to core, and the run spread
((max − min) / median):

**SHA-1** — median MB/s over thread CPU time, ratio to core, and the run spread ((max − min) / median):

| Workload | core | first-party block (control) | sha1dc | CommonCrypto | QCryptographicHash |
|---|---|---|---|---|---|
| W1: 64 MiB, one update | 243 (1.00x; 14%) | 403 (1.66x; 21%) | 629 (2.59x; 11%) | 2,581 (10.64x; 16%) | 960 (3.96x; 17%) |
| W2: 20,015 bodies of 400 B | 199 (1.00x; 36%, 40.3 ms) | 310 (1.56x; 34%, 25.9 ms) | 588 (2.96x; 48%, 13.6 ms) | 1,083 (5.45x; 34%, 7.4 ms) | 884 (4.45x; 45%, 9.1 ms) |
| W3: 1 MiB | 280 (1.00x; 31%) | 476 (1.70x; 1%) | 727 (2.59x; 4%) | 2,952 (10.54x; 1%) | 1,095 (3.91x; 1%) |
| W3: 16 MiB | 228 (1.00x; 31%) | 364 (1.60x; 29%) | 582 (2.55x; 31%) | 2,959 (12.97x; 27%) | 912 (4.00x; 21%) |
| W3: 256 MiB | 230 (1.00x; 8%) | 404 (1.76x; 18%) | 631 (2.74x; 21%) | 2,819 (12.26x; 15%) | 956 (4.16x; 8%) |

**SHA-256** — median MB/s over thread CPU time, ratio to core, and the run spread ((max − min) / median):

| Workload | core | HACL* | CommonCrypto | QCryptographicHash |
|---|---|---|---|---|
| W1: 64 MiB, one update | 333 (1.00x; 22%) | 494 (1.48x; 17%) | 2,842 (8.54x; 24%) | 211 (0.63x; 5%) |
| W2: 20,015 bodies of 400 B | 332 (1.00x; 69%, 24.1 ms) | 393 (1.18x; 27%, 20.4 ms) | 2,390 (7.19x; 75%, 3.3 ms) | 185 (0.56x; 27%, 43.4 ms) |
| W3: 1 MiB | 405 (1.00x; 22%) | 516 (1.27x; 54%) | 2,879 (7.11x; 46%) | 218 (0.54x; 26%) |
| W3: 16 MiB | 356 (1.00x; 29%) | 491 (1.38x; 45%) | 2,826 (7.93x; 20%) | 186 (0.52x; 25%) |
| W3: 256 MiB | 354 (1.00x; 9%) | 475 (1.34x; 8%) | 2,764 (7.81x; 12%) | 201 (0.57x; 7%) |

W2 is the hashing part of a first observation at 10k: 24.1 ms of core SHA-256
against 3.3 ms through CommonCrypto in run 2 (27.9 ms against 3.5 ms in run 1).

## Window-thread baseline

`application_feedback_counter_costs` in the same tree, first observation
(`observe_cold`), wall clock as the row measures it, under the load described
above:

| Run | Workload | Bodies hashed | Median | Least | Most |
|---|---|---|---|---|---|
| 2 (`3f9e45980`) | 1k | 2,015 | 4.40 ms | 4.31 ms | 5.07 ms |
| 2 | 10k | 20,015 | 44.7 ms | 44.3 ms | 55.6 ms |
| 1 (`40870b777`) | 1k | 2,015 | 4.35 ms | 4.27 ms | 5.07 ms |
| 1 | 10k | 20,015 | 123.4 ms | 43.4 ms | 253.0 ms |

The two end-to-end rows ran under load at `6b0c7fb15` (main plus this
evaluation, which changes no product code) in the same Release tree, `-j1`:

| Row | Result | Raw |
|---|---|---|
| `application_first_tab_comparison` | Passed (`FirstTabExact`): connect 103 ms, paged review and Git load 2,681 ms, base 1,710 ms, head 1,662 ms, file 2,210 ms; longest pulse 51 ms | [first-tab](RFC-BPR-0080/macos-arm64-first-tab-comparison.json) |
| `application_large_review_responsiveness` | **Failed twice, on main's own behavior:** "Down from a line out of view did not reveal the next" (`keyboard_line_reveal` expired at the 90 s course clock). Every phase before it was measured: composition 1,223 ms, main to home render 1,230 ms, largest wheel event 88 ms, no input-budget failure | [responsiveness](RFC-BPR-0080/macos-arm64-large-review-responsiveness.json) |

The responsiveness failure is keyboard navigation in the comparison, not
hashing; this branch touches no product code, so it is main's state at
`6b0c7fb15` and is reported for its owner. Run 1's 10k spread above is far
wider than the 10 ms the host-port rule is about, and every row shares a
loaded machine, so these record the rows rather than set the baseline for that
rule. W2 above puts the hashing inside the 10k first
observation at about 24 to 28 ms of CPU. [[BPR-1447]] measures before and after
in one session on one tree, with Review Cycle capture and export of a 16 MiB
file and a syntax request for a 16 MiB document: a baseline taken in a
different session could not show a 10 ms change. This evaluation changes no product code, so no row can have regressed.

## Library fit

What the built static libraries import and export (`nm` on the Release
objects), against the purity bar: no I/O, threads, clocks or mutable
globals.

| | sha1collisiondetection | HACL* `Hacl_Hash_SHA2` |
|---|---|---|
| Pinned source | `cr-marcstevens/sha1collisiondetection` `a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988` (codeload tarball, 787,039 bytes, SHA-256 `b9c89e19…1729`) | `hacl-star/hacl-star` `504c2987452f87fe44bce9b9f12e19d6e051761f` (codeload tarball, 10,228,408 bytes, SHA-256 `e32f7add…c52c`), `dist/gcc-compatible` (`msvc-compatible` for MSVC) |
| Licence | MIT (`LICENSE.txt`, SHA-256 `9daac99c…c83d`) | MIT (`dist/LICENSE.txt`, SHA-256 `998ce04f…d78c`) |
| Compiled sources | `lib/sha1.c`, `lib/ubc_check.c` (2,283 lines of C) | `Hacl_Hash_SHA2.c` (SHA-224/256/384/512; only SHA-256 is called) |
| Imports | `memcpy`; `abort`, from a `switch` default for a step index that cannot occur | `memcpy`, `malloc`, `calloc`, `free` (only its streaming API allocates; the block-level calls used here do not), and the compiler's stack-protector and fortify symbols |
| Headers it needs | `<stddef.h>`, `<stdint.h>`, `<stdlib.h>`, `<string.h>` through `SHA1DC_NO_STANDARD_INCLUDES`, as Git builds it | KaRaMeL's `krml` headers, which include `<stdio.h>` and `<stdlib.h>`; no I/O function is referenced |
| Globals | `sha1_dvs`, the disturbance-vector table: exported and not declared `const`, though nothing writes it | Constant tables only |
| State | All in the caller's `SHA1_CTX` (2,400 bytes) | All in the caller's 8 words |
| Exported names | Unprefixed: `SHA1DCInit`, `SHA1DCUpdate`, `SHA1DCFinal`, `ubc_check`, `sha1_dvs`, … (11) | `Hacl_`-prefixed (43) |
| API notes | `SHA1DCFinal` returns nonzero on detection; Git's configuration (`SHA1DC_INIT_SAFE_HASH_DEFAULT=0`) keeps the digest plain SHA-1 | The allocation-free calls are in `internal/Hacl_Hash_SHA2.h`, not the public header, and take the input through a non-`const` pointer they never write; a 32-bit length per call |
| Acceleration | None | None |

The wrappers are in `tests/hash_evaluation/hash_arms.cpp`. The HACL* wrapper
keeps the core `sha256_stream` shape (eight state words, one 64-byte block,
a byte count) and calls `Hacl_Hash_SHA2_sha256_update_nblocks`,
`_update_last` and `_finish`, so a stream never allocates: the RFC's
`noexcept` stream condition holds for that shape.

## Host port

[[BPR-1447]] built the first host-port site: the feedback classifier's body
digests. The window read host digests a fetched snapshot's feedback bodies
through `application::body_digest_port` on the GitHub provider's effect
worker, before the snapshot is posted to the decision lane. The digests travel
with the snapshot, and the core uses a supplied digest instead of hashing.
The backend is CommonCrypto on macOS and the core's SHA-256 elsewhere until
[[BPR-1448]] and [[BPR-1449]]. `application_body_digest_port` checks every
backend against the core for the FIPS answers, every length from 0 to 200,
block and 1 MiB edges and seeded inputs. It also checks that a snapshot with
supplied digests gives the same units and counts as one the core hashes,
with no body hashed by the core, and that a missing or mismatched digest
falls back to the core.

Release tree, `-j1`, each row twice on the claim commit `a17294297` (before)
and with the port (after), loaded machine (1-minute load in brackets).
`application_feedback_counter_costs`, medians, with least and most in brackets:

| Phase | Workload | Before | After | Core hashed on the window thread |
|---|---|---|---|---|
| `observe_cold`, no digests (as before) | 1k | 3.41 / 3.41 ms [11.9, 3.5] | 3.22 / 3.18 ms [5.4, 1.5] | 2,015 bodies, 805 KB |
| `observe_cold_supplied`, digests from the port | 1k | (no such path) | 1.58 / 1.58 ms | none |
| `observe_cold`, no digests | 10k | 34.1 / 34.5 ms (34.0 to 34.7) | 33.3 / 32.0 ms (31.5 to 34.2) | 20,015 bodies, 8.04 MB |
| `observe_cold_supplied` | 10k | (no such path) | 18.0 / 17.2 ms (17.1 to 19.0) | none |
| `digest_bodies`, core backend, off the window thread | 10k | | 18.6 / 18.1 ms | |
| `digest_bodies`, CommonCrypto, off the window thread | 10k | | 8.2 / 7.5 ms | |
| `digest_bodies`, core / CommonCrypto | 1k | | 1.81 / 1.75 ms, 0.73 / 0.74 ms | |

The end-to-end rows, before | after, ms, runs 1 and 2:

| Row | Load | Result | Phases |
|---|---|---|---|
| `application_first_tab_comparison` | 11.0, 3.5 \| 5.4, 1.5 | passed all four runs | paged load 1194, 1227 \| 1190, 1160; base 668, 636 \| 588, 610; head 615, 655 \| 589, 579; file 814, 783 \| 739, 735 |
| `application_large_review_responsiveness` | 10.3, 3.6 \| 5.1, 1.5 | failed all four runs on main's keyboard reveal ("Down from a line out of view did not reveal the next") | paged load 1250, 1222 \| 1202, 1183; large file selection 555, 565 \| 538, 536; Overview columns 3 / 12 / 0: 3, 3 / 4, 6 / 3, 8 \| 7, 7 / 8, 8 / 7, 8 |

**Decision: adopt on macOS.** The rule's throughput half was already met
(7.11x to 8.54x core on 1 MiB or more, run 2). At this site, CommonCrypto digests the 10k
bodies (about 400 bytes each) 2.4x faster than the core. The site half also
passes on both counts. The first 10k observation's window-thread time falls
by about 15 ms, from 32.0 to 33.3 ms to 17.2 to 18.0 ms, and the 8.04 MB it hashed moves to the
provider's worker, where CommonCrypto takes 7.5 to 8.2 ms. No body is hashed
on the window thread when the snapshot carries digests. Every other phase
is flat or lower, except the Overview column phases, which are 3 to 4 ms
higher after the change in both runs. They read kept tallies only, through
code this change does not touch, so the difference is recorded rather than
explained. Windows and Linux keep the core backend until their stations
measure CNG and libcrypto. The Review Cycle 16 MiB capture and syntax rows
named above are not this site and were not run.

## What adoption still needs

- **sha1dc ([[BPR-1450]], after a Q1 Accept).** Link it as its own pinned
  target, refuse a detection everywhere a SHA-1 identity is produced, run the
  existing digest-using rows, and settle the AC03 finding above. Its exported
  non-`const` `sha1_dvs` table and unprefixed names are worth a line in the
  owner's decision: the table is never written, and nothing else in the
  product links sha1dc today. On a Reject, [[BPR-1452]] makes the first-party
  SHA-1 hash whole blocks and the report states the remaining gap.
- **HACL* ([[BPR-1451]], after a Q2 Accept).** Wrap the block-level calls from
  `internal/Hacl_Hash_SHA2.h` exactly as the evaluation does, so the stream
  never allocates, and run the digest-using rows. The streaming API is not
  used, and the `internal/` header is not HACL*'s public API, so a later pin
  update must recheck it.
- **Host port ([[BPR-1447]]).** Built and adopted on macOS for the feedback
  body digests; see "Host port" above.
- **Stations ([[BPR-1448]], [[BPR-1449]]).** Run the same driver, add the CNG
  or libcrypto arm, and re-apply every rule with their rows. The Linux station
  is expected to lack SHA-NI, so its rows are the fallback-path test of the
  0.9x condition.
- **Every adoption.** The RFC's packaging (provenance, licence,
  `dependencies.json`, notices, SPDX, corresponding source) and the
  `architecture_boundaries` allow-list change happen only with the owner's
  Accept.

## Raw results

Each file is the driver's complete output: the machine, the build, every line
the harness printed and the driver's own comparisons with Git. No path or
content from outside the repository and its seeded inputs is recorded.

| File | Revision | Phases |
|---|---|---|
| [macos-arm64-correctness.json](RFC-BPR-0080/macos-arm64-correctness.json) | `d9bfa436b` | known answers, chunking, collisions, differential, Git, both repository sweeps |
| [macos-arm64-throughput-run2.json](RFC-BPR-0080/macos-arm64-throughput-run2.json) | `3f9e45980` | W1 to W3, window-thread row |
| [macos-arm64-throughput-run1.json](RFC-BPR-0080/macos-arm64-throughput-run1.json) | `40870b777` | W1 to W3, window-thread row |
| [macos-arm64-first-tab-comparison.json](RFC-BPR-0080/macos-arm64-first-tab-comparison.json), [macos-arm64-large-review-responsiveness.json](RFC-BPR-0080/macos-arm64-large-review-responsiveness.json) | `6b0c7fb15` | the two end-to-end window-thread rows |
| [macos-x86_64-rosetta-correctness.json](RFC-BPR-0080/macos-x86_64-rosetta-correctness.json) | `d9bfa436b` harness | known answers, chunking, collisions, differential, Git |

`dirty` is true in the throughput and x86_64 files because documentation was being
edited in the working tree; the harness sources were the committed ones.

## Deterministic core platform SHA-256 — owner r4/r5, 2026-10-18

[[BPR-1617]] implements the [adopted architecture amendment](../rfcs/amendments/RFC-BPR-0080-platform-crypto-r4.md).
The owner directly selected CommonCrypto on macOS and CNG on Windows with
pinned HACL as the portable fallback; this selection supersedes the earlier
claim that OS CPU dispatch itself breaks determinism. All digest bytes,
SHA-1 collision detection, host scheduling and persisted identities retain
their contracts. Chrono duration/time-point arithmetic and caller-owned
explicitly seeded PRNG state are admitted; clock reads, implicit entropy and
global C RNG state remain prohibited. No generator or RE2 placement changes.

The public SHA-256 API remains noexcept. Copies duplicate the unfinished
prefix; moves transfer ownership. CommonCrypto uses an inline context and
HACL uses its existing block API. CNG uses the Windows 10+ SHA-256 pseudo-handle
and owns a hash handle per stream; construction/copy may allocate, while
updates allocate no first-party memory. Failed native initialization chooses
HACL before input. Failed native duplication/update/finalization terminates;
the API never returns a digest for a partially processed input. Backend
failure contracts use executable CNG stubs, explicitly separate from native
Windows observations.

### Direct observations

- macOS arm64, Apple M4 Pro, Darwin 25.6.0, AppleClang 21.0.0: the public
  `core_sha256` course passed known answers, block/padding edges, every split
  point, empty updates, independent copies, assignments and moves. LLDB stopped
  in `libcommonCrypto.dylib`'s `CC_SHA256_Update` with the stack
  `core::sha256 -> sha256_stream::update -> update_platform`; the traced child
  was then stopped by its own PID. This demonstrates actual native dispatch.
- Native JaegAW Windows 11 Home 10.0.26200 x64, Intel i9-10900KF, MSVC
  19.51.36260.0, SDK 10.0.26100.0: the exact public header, stream wrapper,
  CNG/portable sources and SHA-256 test file were transferred to one owned
  temporary directory. A standalone CMake Release build linked the pinned
  `dist/msvc-compatible` HACL source and system bcrypt, using `/W4 /WX
  /permissive-` for the course. Both native and portable courses passed,
  including independent copies, assignments and moves. `dumpbin /imports`
  showed bcrypt.dll and CreateHash, DuplicateHash, HashData, FinishHash and
  DestroyHash. Native/portable timing below also discriminates dispatch.
  Remote source SHA-256 values matched local bytes exactly; the committed
  [evidence manifest](_BPR-1617-platform-sha256.json) retains them. No Windows
  repository checkout or owner desktop was used. MSBuild's MSB8029 warning
  concerns its temporary output location; compilation and both courses passed.
- The new CNG contract runs the exact production source against an API stub:
  deep-copy independence and handle release, initialization-to-HACL fallback,
  and fatal refusal at duplication/update/finalization. It is not a substitute
  for the native Windows course above.
- The boundary contracts permit chrono arithmetic/time points and explicit
  seeded engines, reject clock reads (including aliases), random_device and
  global C RNGs, and reject unrelated crypto/clock capabilities even inside
  the two native SHA-256 source owners. The bcrypt link is Windows-branch-only.

### Compact bulk probe

Release probe through `core::sha256`, one update of 64 MiB, median of seven;
input byte `i` is `uint8_t(i * 131 + (i >> 7))`. Input preparation is outside
the timed interval; calls include stream creation/finalization. These are
loaded-machine observations, not the original full station evaluation.

| Platform | Native median / MB/s | HACL median / MB/s | Native/HACL |
|---|---|---|---|
| M4 Pro, AppleClang `-O3` | 21.352 ms / 3143.0 | 121.209 ms / 553.7 | 5.68x |
| i9-10900KF, MSVC Release `/O2` | 121.502 ms / 552.3 | 247.165 ms / 271.5 | 2.03x |

All four outputs equal
`fb8c3f34e6dc82d617c642e906c2d0ed1faf93041a7e4cc0e235b2cbd3fa4735`.
The probe compiles `sha256_hacl.cpp` plus exactly one native/portable source
and the same pinned HACL object; no alternate hash implementation enters it.

### Verification and remaining scope

The focused core-only pass covers the public SHA-256 course, Git object
identities and collision-vector refusals, the core body-digest port, CNG
contract branches and architecture boundaries. Formatting uses clang-format
22.1.8. Documentation verification and its 67 contract tests pass. The full
architecture contract initially passed 157/158; its one failure was the new
ownership fixture omitting its target source list. Adding that list made the
focused contract pass; no production-boundary relaxation was needed.
Final exact-candidate results are recorded in BPR-1617's checkpoint.

RFC-BPR-0080 remains In Progress. BPR-1448 still owes its full station
SHA-1/collision/repository sweep, W1-W3 and host window-thread course; the
focused CNG course here is partial native correctness evidence. BPR-1449's
Linux libcrypto evaluation and Q3 remain pending. BPR-1453 retains full RFC
acceptance/package closeout, including the original dev-build and regression
obligations. No Linux or universal-x86 slice result, full dev build, release
qualification or owner judgement is claimed by this amendment milestone.
