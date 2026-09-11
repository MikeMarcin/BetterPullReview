---
schema: "implement-rfc.card.v1"
id: "BPR-0719"
title: "A large file with a small change is refused by the comparison work limit"
status: "Ready"
rfcs: ["RFC-BPR-0004", "RFC-BPR-0013"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
---

# A large file with a small change is refused by the comparison work limit

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md),
reviewing microsoft/terminal#20607 signed out: "Seems like I cannot open the
ScreenBufferTests.cpp file in the review but I can open some other ones. …
Ah I see there is some kind of limit that prevents us from seeing the one
file. That's not going to work. We cannot have limits like that we need to
be able to see every file in the reviews." The screenshot on the decision
shows `src/host/ut_host/ScreenBufferTests.cpp` (M, +291 −0) selected and
the comparison pane reading "Comparison unavailable — file exceeds the
comparison work limit (cached)" while the three smaller files render.

## Cause

The core decides the refusal in `src/core/src/diff_compute.cpp`. The primary
alignment (`lcs_matches`, around lines 399-430) builds a full LCS table of
(left lines + 1) × (right lines + 1) cells without trimming the common
prefix and suffix, and charges it against
`MaximumMatrixCells = 16U * 1024U * 1024U`
(`src/core/include/bpr/core/diff_compute.hpp:74`). Any file over about
4,096 lines on both sides exhausts it however small the change; the terminal
test file is roughly 8,000 lines with a pure insertion. The only fallback,
`ordered_unique_matches` (lines 600-647, from 10520a43), gives up as soon as
any right-hand line repeats, which every real source file does (`}`, blank
lines), so the result is `compute_limit` (lines 1266-1269); the intraline
pass can refuse the same way (lines 1392-1396). There is no plain textual
fallback. The Qt workbench only presents the reason
(`src/adapters/qt/src/diff_workbench.cpp:620-621`) and appends "(cached)"
when the application's in-memory result cache answered
(`src/application/src/diff_application_host.cpp:782-820`, 64 entries keyed by
the context, not by the limits). The limits arrived with 0e35476e and the
reason code with 15fbf95b (both 2026-08-01); neither was ever an accepted
product behaviour in front of the owner.

## Scope

- No changed file in a pull request is refused for its size alone. The
  alignment trims the common prefix and suffix before any table is built, so
  the work is proportional to the changed region; when the exact alignment
  would still exceed the bound, the file falls back to a cheaper linear
  alignment (patience or Myers over the remaining region) rather than a
  refusal, and the status line says the comparison is approximate if it is.
- The RFC-BPR-0013 resource limits stay for genuinely unbounded input
  (`MaximumLinesPerSide`, `MaximumBytesPerSide`); the change is that a limit
  degrades the alignment instead of refusing the file, and the limit values
  are reviewed against real repositories rather than 4k-line files.
- A cached `compute_limit` result does not outlive the limit that produced
  it: the result key or the cache clear covers the change.
- Owned paths: `src/core/src/diff_compute.cpp`,
  `src/core/include/bpr/core/diff_compute.hpp`,
  `src/application/src/diff_application_host.cpp`, `tests/core` diff compute
  tests and the application diff host tests. Core decides, host performs, Qt
  presents; the Qt workbench needs no change unless a new status wording is
  introduced.

## Acceptance

- A core test with one side of about 8,000 lines that repeat, and a pure
  291-line insertion, aligns exactly and reports every inserted line; the
  same input at HEAD reproduces `compute_limit` first.
- A core test whose changed region alone exceeds the bound receives an
  aligned result marked approximate, never a refusal.
- `ctest -R 'diff_compute|diff_application_host|application_composition_contract'`
  and the public-path courses that pin diff evidence pass; `check_boundaries`,
  strict tidy on the touched translation units and `clang-format --Werror`
  are clean.
- On the signed bundle the owner opens ScreenBufferTests.cpp in
  microsoft/terminal#20607 and sees the diff; this is asked in the next
  attended session rather than as a criterion the tests can close.
