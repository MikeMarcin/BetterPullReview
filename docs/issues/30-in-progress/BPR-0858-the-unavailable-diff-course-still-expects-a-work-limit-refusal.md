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
