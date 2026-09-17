---
schema: "implement-rfc.card.v1"
id: "BPR-0858"
title: "application_unavailable_diff_public_path still expects the work-limit refusal BPR-0719 retired"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0858: application_unavailable_diff_public_path still expects the work-limit refusal BPR-0719 retired

## Defect

`application_unavailable_diff_public_path` fails with
`large-review-benchmark-failed: real host result must exceed the retained comparison limit`.
It fails on Linux at `a1864fa2` and on macOS at `20d99914`. The macOS
evidence shows `PulsesWithDiffWork:130` and `UnavailableReasonVerified:false`.

The assertion, `tests/end_to_end/large_review_benchmark.cpp` ~885-895,
requires two things of `large/comparison.cpp`:

- `Alignment.Content == unavailable`, with `compute_limit`;
- the status "Comparison unavailable — file exceeds the comparison work limit".

`901a9c06` ("Finish BPR-0719", 2026-09-10) changed that behaviour on purpose.
A work limit now degrades the alignment: the result is text marked
`AlignmentApproximate`. `compute_limit` remains only for a
`MaximumSegments` overflow. That commit updated the core and host tests but
not this course.

## Scope

**Integrator decision.** The course exists to cover the unavailable state
through the public path, so keep it an unavailable course. Use, or generate,
a fixture file that overflows `MaximumSegments`, and assert the status text
the product shows for that.

- Check that the approximate-alignment path already has public-path coverage
  from BPR-0719. If it does not, add the approximate assertion to the
  large-review benchmark as well.
- If the fixture change moves the round-trip pins (`src/app`), reseal with a
  recorded review.

## Acceptance

- The row passes on macOS and Linux, asserting the unavailable state for a
  segment overflow.
- The approximate-alignment public path is covered and named here.

## Scout findings (dsp-004, read-only)

**Why the course fails.** The course's `--unavailable` branch sets only
`DiffComputeLimits.MaximumPrimaryIndexWork = 0U`
(`large_review_benchmark.cpp` ~848-852). Since BPR-0719 that gives an
approximate alignment.

**When `compute_limit` still applies.** `compute_file_diff` returns
`unavailable`/`compute_limit` only when the primary segments are absent or
exceed `MaximumSegments` (`diff_compute.cpp` ~1428-1431). The workbench still
shows the same status text for it (`diff_workbench.cpp` ~689).

**Why the stored fixture cannot be enlarged instead.** `large/comparison.cpp`
is 20,000 lines with about 20 single-line replacements. A file that exceeds
the default `MaximumSegments` (1,000,000) would first hit
`MaximumLinesPerSide` (200,000), and so `materialization_limit`.

**Smallest faithful change.** Set `DiffComputeLimits.MaximumSegments = 1U` in
the course's fixture options; the host accepts 1 up to the default. The
existing assertions at ~892-894 stay unchanged, and no fixture file or pin
moves.

**Approximate alignment has no public-path coverage.** It is covered only by
core, application-host and Qt tests: `901a9c06` added no end-to-end course.
The acceptance's second item therefore needs a new course, for example an
`--approximate` argument that asserts `AlignmentApproximate` and its status
line.

**Registration.** The row is registered on every platform, `RUN_SERIAL`, and
shares its executable with `application_large_review_benchmark`, which the
`--unavailable` branch does not affect.

## Fix (dsp-004, Mac)

**The unavailable course now proves a segment overflow.** Its test-only
limits set `DiffComputeLimits.MaximumSegments = 2`. With default limits,
`large/comparison.cpp` aligns in 40 segments and the recovery file
`src/file_0031.cpp` in 2. The large file is therefore refused with
`unavailable`/`compute_limit` and "Comparison unavailable — file exceeds the
comparison work limit", while recovery still computes. The scouted limit of 1
refused the recovery file too (`build/r2-logs/dsp4-0858-limit1-unavailable.log`).

**A second fault was hidden behind the first.** The review opens on Overview,
which replaces the comparison, so the status label was hidden and "unavailable
message must support copying" failed. The course now enters Read through the
strip's public modality action before selecting a file. Every original
assertion is unchanged, and the course records the recovery file's segment
count against the limit.

**Approximate-alignment coverage.** The new
`application_approximate_diff_public_path` row runs the same executable with
`--approximate`. It is registered on all platforms, network isolated,
`RUN_SERIAL`, with `TIMEOUT 120`. It sets `MaximumPrimaryIndexWork = 0`,
enters Read, selects `large/comparison.cpp`, and asserts:

- a text comparison with no diagnostic;
- `AlignmentApproximate`;
- the exact status "Diff ready — N semantic changes (approximate alignment)",
  with N non-zero;
- both panes holding the exact source.

Negative control: with default limits the course fails at "work-limited host
result must be marked approximate alignment"
(`dsp4-0858-negative-approximate.log`).

**Unchanged.** No product source, fixture file or round-trip pin moved. The
fixture README and `docs/verification/fixture-catalog.md` describe both rows.

**Runs (macOS).**

- Both rows pass 3/3: unavailable in 14.7-15.5 s, approximate in 13.0-13.3 s
  (`dsp4-0858-{unavailable,approximate}-run{1,2,3}.log`).
- `application_large_review_benchmark`, `test_network_isolation_manifest`,
  `architecture_boundaries` and the documentation gate pass.

**Still open.** The Linux run of both rows.

## Linux runs at the merged head (dsp-005, bpr-linux/int-12)

Station `bpr-linux` (JaegAW, WSL2), branch `linux-lane`, at
`754d05387286b201d2afb13757cd894bccfddcbd` — the merged head, equal to `main`
— on a clean tree, after a stock `cmake --preset dev --fresh` configure and a
`--clean-first` build that was warning-clean under GCC 15.2.0. Each row was
its own serial run, `ctest --test-dir build/dev -j1 -V -R '^<name>$'`, from
18:51:50Z. The Secret Service `login` collection read `(<false>,)` before the
group. Both rows are offscreen by registration and, on Linux, also carry
`QT_STYLE_OVERRIDE=Windows`.

| Row | Result | Time | Marker |
|---|---|---|---|
| `application_unavailable_diff_public_path` | Pass | 3.82 s | `UNAVAILABLE_DIFF_COMPOSED_OK` |
| `application_approximate_diff_public_path` | Pass | 3.69 s | `APPROXIMATE_DIFF_COMPOSED_OK` |

So both halves of the acceptance hold on Linux: the unavailable course proves
the segment overflow with `MaximumSegments = 2`, entering Read through the
strip's public modality action, and the new approximate course covers the
alignment path BPR-0719 left uncovered. Linux is faster than the macOS
integrator here (3.82 s and 3.69 s against 14.7-15.5 s and 13.0-13.3 s); both
run well inside the rows' 120 s timeout.

Each log carries four `This plugin does not support propagateSizeHints()`
lines. That is the offscreen Qt platform plugin, not a product diagnostic, and
it appears on every offscreen public-path row on this station. The run record
is in `docs/verification/dsp4-linux-confirmation.md`.

## Windows run at the merged head, 2026-09-17 (bpr-win, task-20260917-win-merged-head-runs)

The head is `2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`. The full row-by-row sweep
(362 rows, 309 passed, 53 failed) is `docs/verification/dsp4-windows-merged-head-sweep.md`.

Both rows still fail on Windows, and not for this card's reason. Each ends at
`large-review-benchmark-failed: application startup succeeds`, because the
application refuses to start: `profile_startup_failure` **2 =
`registry_unavailable`**, measured with a temporary print at
`github_application.cpp` ~12623 that was reverted. The refusal is the data root
the Windows branch of the benchmark uses, so it is [[BPR-0856]]'s `#if` rather
than the segment-limit work here; see that card and the sweep record. The
course never reaches its diff assertions, so this card's own change is
unmeasured on Windows.
