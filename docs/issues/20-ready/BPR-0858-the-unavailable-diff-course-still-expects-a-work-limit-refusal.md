---
schema: "implement-rfc.card.v1"
id: "BPR-0858"
title: "application_unavailable_diff_public_path still expects the work-limit refusal BPR-0719 retired"
status: "Ready"
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
