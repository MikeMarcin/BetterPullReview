---
schema: "implement-rfc.card.v1"
id: "BPR-0719"
title: "A large file with a small change is refused by the comparison work limit"
status: "Done"
rfcs: ["RFC-BPR-0004", "RFC-BPR-0013"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
acceptance: [{"criterion":"A core test with one side of about 8,000 lines that repeat and a pure 291-line insertion aligns exactly and reports every inserted line; the same input at HEAD reproduces compute_limit first","outcome":"pass","revision":"6de81ee8","evidence":{"kind":"command","detail":"tests/core/diff_compute_tests.cpp test_large_files_degrade_instead_of_refusing: 8,000 lines cycling five repeated forms, 291 of them inserted at line 4,000; the result is exact (AlignmentApproximate false), one group, 291 right-only lines, nothing removed, within MaximumMatrixCells. Built and run against HEAD core first (build/core-only, header field only): FAILED 'a repeated 8,000-line file with a pure insertion computes under default limits' (the compute_limit refusal). After the change build/core-only and build/dev core_diff_compute pass (scratch logs core-build-1/2.log, ctest-0719.log, ctest-0722.log)."}},{"criterion":"A core test whose changed region alone exceeds the bound receives an aligned result marked approximate, never a refusal","outcome":"pass","revision":"6de81ee8","evidence":{"kind":"command","detail":"Same test: two 5,002-line sides whose 5,000 repeated middle lines are phase-shifted with distinct opening and closing lines, so trimming leaves a 5,000x5,000 region (25M cells > 16M), ordered-unique declines on repeats and patience finds no anchor; the outcome is a validated text result with AlignmentApproximate true. The over-budget cases in test_moves_identity_and_limits and test_ordered_unique_line_fallback (matrix cells one below need, index budget one below need, MaximumMatrixCells 1 with pairing) all return text results marked approximate; application_diff_host's MaximumMatrixCells=1 case publishes a validated approximate text result (tests/application/diff_application_host_tests.cpp). The core never returns compute_limit for size; only a MaximumSegments overflow keeps that code."}},{"criterion":"ctest -R 'diff_compute|diff_application_host|application_composition_contract' and the public-path courses that pin diff evidence pass; check_boundaries, strict tidy on the touched translation units and clang-format --Werror are clean","outcome":"pass","revision":"6de81ee8","evidence":{"kind":"command","detail":"build/dev: core_diff_compute, application_diff_host, application_composition_contract (147.6 s and 156.1 s), application_review_round_trip_composition_contract, application_diff_workbench_public_path and qt_composed_github_scale_100 pass (scratch ctest-0719.log at the BPR-0719-only binary; core_diff_compute, application_diff_host and both composition contracts again in ctest-0722.log). cmake --target check_boundaries: both boundary scripts verified. _coding-standard/scripts/tidy.py --warnings-as-errors on src/core/src/diff_compute.cpp, src/adapters/qt/src/diff_workbench.cpp, tests/core/diff_compute_tests.cpp and tests/application/diff_application_host_tests.cpp: exit 0. clang-format --dry-run --Werror on every touched file: exit 0. application_uiforetw_selected_diff_contract fails with STARTUP_FAILED at 0.5 s, and fails identically on a clean HEAD worktree build (scratch head-uiforetw.log), so it is a pre-existing red at HEAD and is carded separately, not attributed to this change. No cache change was needed: the in-memory diff cache never stores failures, and the core no longer produces a size refusal to cache."}},{"criterion":"On the signed bundle the owner opens ScreenBufferTests.cpp in microsoft/terminal#20607 and sees the diff","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0719-large-file-diff, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 1274b11107354f150c4184d536982d4a1b014300cf90ece06476ee9cc74f3032, decision 9c72aecf-7bcb-4695-b612-c8475759d639, actor human, 2026-09-11T18:38:23Z: accept. The owner's screenshots on the BPR-0646 and BPR-0720 decisions show src/host/ut_host/ScreenBufferTests.cpp (VtScrollMarginsNewlineColor, VtResizePreservingAttributes) rendered in both panes with 'Diff ready — 2 semantic changes' and no '(approximate alignment)' suffix, and the comment thread anchored at ScreenBufferTests.cpp:1576 (captures/owner-read-chrome-2026-09-11.png, captures/owner-dark-palette-2026-09-11.png)."}}]
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

## Attended candidate, 2026-09-10 (v004)

The pending criterion above is candidate `BPR-0719-*` in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md)
on the bundle built at 3c9a3f1c (executable sha256 `9261509a…`), candidate
revision `app-release-9261509a-2026-09-10.3`; prepared, undecided, no host
running until the owner asks for the session.

## Attended candidate, 2026-09-11 (v005)

The pending criterion above is candidate `BPR-0719-large-file-diff` in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`;
prepared, undecided, no host running until the owner asks for the session.
The v004 packet is historical.

## Attended candidate, 2026-09-11 (v006)

The pending criterion above is candidate `BPR-0719-large-file-diff` in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
on the bundle built from the source at d1ba791f (executable sha256
`cc2afa7f…`), candidate revision `app-release-cc2afa7f-2026-09-11.2`;
prepared, undecided, no host running until the owner asks for the session.
The ask is unchanged; the v005 packet is historical.

## Attended verdict, 2026-09-11 (v006): accepted

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; the session was handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0719-large-file-diff`, decision `9c72aecf`, 18:38:23Z: **accept**, no note. The
owner's screenshots on the BPR-0646 and BPR-0720 decisions show `ScreenBufferTests.cpp` in both
panes with "Diff ready — 2 semantic changes" and no "(approximate alignment)" suffix
([owner-read-chrome-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-read-chrome-2026-09-11.png),
[owner-dark-palette-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-dark-palette-2026-09-11.png)). Done.
