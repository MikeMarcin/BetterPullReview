---
schema: "implement-rfc.card.v1"
id: "BPR-1446"
title: "Evaluate sha1dc, HACL* SHA-2 and platform crypto on macOS arm64"
status: "Blocked"
rfcs: ["RFC-BPR-0080"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
blocked_by: "idle-macos-station"
---

# BPR-1446: Evaluate sha1dc, HACL* SHA-2 and platform crypto on macOS arm64

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`hash-evaluation-macos`.

## Outcome

An opt-in, non-shipping evaluation target measures the RFC's candidates
against the current core SHA-1 and SHA-256 on this Mac, and a committed report
applies the decision rules. Nothing enters core, `third_party/`,
`packaging/dependencies.json` or a package. The shipping configuration acquires
nothing new.

## Acceptance

- **AC1 (RFC AC01, macOS):** A committed report records raw results, the CPU,
  its SHA-extension flags, the OS, the compiler and the build, applies the
  decision rules, and gives one adopt, reject or pending decision per
  component. Q1, Q2 and Q3 are recorded as unanswered; a passing candidate is
  "eligible pending Qn".
- **AC2 (RFC AC02 gate before adoption):** Zero mismatches for every candidate
  across the NIST CAVP byte-oriented SHA-1 and SHA-256 vectors, boundary
  lengths, split points and uneven chunking, seeded differential inputs up to
  256 MiB against the baseline and CommonCrypto, `git hash-object` in both
  object formats, trees and commits against `mktree`/`write-tree`/
  `commit-tree`, and every object of this repository's history.
- **AC3 (RFC AC03 evidence before adoption):** sha1dc detects the SHAttered
  pair, the SHA-mbles pair and its own test vectors while a plain-SHA-1 control
  shows each pair's digests equal; the repository sweep detects nothing. What
  detection does to a collision input hashed as a Git object is recorded.
- **AC4 (RFC AC04, macOS):** W1 to W3 throughput under the RFC's conditions
  (Release `-O3`, idle, `-j1`, medians of 7) for each candidate and the
  baseline, plus the macOS window-thread baseline rows a later adoption is
  compared against.
- **AC5:** The candidates are acquired only through pinned, hash-verified
  archives behind an evaluation-only option, `dev` and `core-only` configure
  without it, and the architecture and documentation checks pass.

## Built and verified (macOS arm64, 2026-10-13)

**Harness.** `BPR_HASH_EVALUATION` (default OFF, in `tests/CMakeLists.txt`)
adds `tests/hash_evaluation`. [`BprHashEvaluationProvider.cmake`](../../../cmake/BprHashEvaluationProvider.cmake)
acquires sha1collisiondetection `a06f1adb`, HACL* `504c2987` and the NIST CAVP
SHA byte vectors as pinned archives (SHA-256 and size checked, licence digests
checked, re-extracted under `build/_bpr/hash-evaluation`).
`bench_hash_evaluation` compares the core hashes (baseline), the control arm
(the core SHA-1 hashing whole blocks), sha1dc as Git configures it, HACL*
through its block-level calls, CommonCrypto and `QCryptographicHash`;
`run_hash_evaluation.py` drives it with Git. Rows `hash_evaluation_known_answers`,
`hash_evaluation_chunking` and `hash_evaluation_collisions` run with the
option on. With it off (`core-only` configured here) nothing is acquired.

**Report.** [RFC-BPR-0080 hash evaluation](../../verification/RFC-BPR-0080.md)
with raw JSON. Decisions: sha1dc eligible pending Q1; HACL* eligible pending
Q2; CommonCrypto passes the host port's throughput condition (7.35x to 9.56x
core on 1 MiB or more), with the site decision left to [[BPR-1447]];
`QCryptographicHash` still rejected for identity and bulk roles; the control
arm measured but not adopted. Windows and Linux not run.

**Correctness** (`d9bfa436b`): zero mismatches for every arm across the CAVP
short, long and Monte vectors and FIPS examples (233 per algorithm), chunking
(335), the seeded differential up to 256 MiB (4,608), 55 Git blobs in both
formats with trees and commits, all 106,013 objects of this repository's
store (sha1dc detected nothing) and `main` converted to SHA-256 (93,019
objects). The x86_64 slice under Rosetta 2 also matched.

**Collisions.** sha1dc detects both SHAttered files and both SHA-mbles files
while plain SHA-1 collides; the reduced-round vector needs reduced-round
detection. As Git blobs the pairs neither collide nor are detected, so the
RFC's four object-level refusal points cannot be shown with public vectors;
recorded for [[BPR-1450]].

**Throughput.** Two runs (`40870b777`; `3f9e45980` with the candidate
libraries hardened like the core). The Mac was shared with other agents'
builds throughout (one-minute load average 45 to 155 on 14 CPUs), so the
RFC's idle-machine condition was not met. Medians of 7 in thread CPU time at
user-interactive QoS, cross-checked by per-run paired ratios: sha1dc 2.55x
to 2.96x core SHA-1 (run 1: 2.06x to 3.03x); HACL* 1.18x to 1.48x core
SHA-256 (run 1: 1.32x to 1.71x); CommonCrypto 7.11x to 8.54x core SHA-256 on
1 MiB or more. Wall-clock times mostly measured descheduling and are kept
only in the raw files.

**Checks.** `coding-format-check` and changed-line clang-tidy clean;
`core_dependency_boundary`, `core_boundary_contract`, `architecture_boundaries`
and `architecture_boundary_contract` pass. `test_network_isolation_manifest`
fails on rows this card did not touch (`application_source_pr_*`,
`gh_cli_git_smart_transport`), from other work on main.

**Independent review** (rfc-reviewer, read-only, at `8e1c287c9`). Not
falsified: real comparisons, sha1dc configured as Git does, a correct
allocation-free HACL* wrapper, the report's ranges and counts, nothing under
`src/`, `third_party/`, `packaging/` or the core allow-list. Fixed in
`3f9e45980`: the driver crashed on MSVC's banner and missed Visual Studio's
configuration folder; a sweep ignored Git's own exit status; the report cited
105,862 objects instead of 106,013 and said the AC03 finding did not affect
the sha1dc decision (a Q1 Accept is now stated as necessary but not
sufficient); the candidates were not built hardened like the core. Two
material findings stand and keep this card open: AC4's idle condition was not
met, and the window-thread rows beyond `application_feedback_counter_costs`
were not measured here.

## Acceptance status (2026-10-13)

| Criterion | Outcome | Evidence |
|---|---|---|
| AC1 | Pass | [Report](../../verification/RFC-BPR-0080.md): decisions per component; the owner's Q1, Q2, collision amendment and idle waiver recorded with their date |
| AC2 | Pass | `macos-arm64-correctness.json` at `d9bfa436b`; x86_64 slice under Rosetta 2 |
| AC3 | Pass | `hash_evaluation_collisions`; the Git-blob finding, settled by the owner's amendment |
| AC4 | Pass under the owner's waiver | W1 to W3: two loaded runs (`40870b777`, `3f9e45980`); the owner waived the idle condition on 2026-10-13. Window-thread rows, all under load: `application_feedback_counter_costs` in both runs; `application_first_tab_comparison` passed; `application_large_review_responsiveness` fails twice on main's own keyboard reveal ("Down from a line out of view did not reveal the next") at `6b0c7fb15`, with every earlier phase measured. This branch changes no product code. |
| AC5 | Pass | `BPR_HASH_EVALUATION` OFF by default; `core-only` configured without acquisition; architecture and documentation checks pass |

## Owner waiver (2026-10-13)

The owner accepted the loaded macOS speed measurements for the adopt rules
("Accept"). The waiver covers load, not missing rows, so the two end-to-end
window-thread rows were built (Release Qt tree, 461 MB) and run under load.
