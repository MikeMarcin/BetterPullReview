---
schema: "implement-rfc.card.v1"
id: "BPR-0874"
title: "Windows: five rows refuse at startup with registry_unavailable off the benchmark's bare temporary data root"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, measuring the integrator's BPR-0856 decision","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0874: Windows: five rows refuse at startup with registry_unavailable off the benchmark's bare temporary data root

## Defect

Five rows fail on the Windows station before they reach any assertion of
their own:

- `application_large_review_benchmark`
- `application_unavailable_diff_public_path`
- `application_approximate_diff_public_path`
- `application_review_ledger_composition_contract`
- `application_defect_composition_contract`

All refuse at startup with `profile_startup_failure 2`, which is
`registry_unavailable`, off the bare `QTemporaryDir` data root the Windows
branch keeps (for example
`C:/Users/mikem/AppData/Local/Temp/better_pull_review_large_review-REVlKc`).

**This corrects an integrator decision, and the correction was a measurement.**
[[BPR-0856]] gave the benchmark an owner-private data root parent **off
Windows only**, and the integrator kept that `#if` on the reasoning that
`%TEMP%` is per-user, so the shared-`/tmp` cause could not arise on Windows.
The cause does not arise — but a different one does, off the same bare root,
and the rows fail just the same.

The station then measured both alternatives, which is what makes this card
actionable rather than a report:

- With the **POSIX arrangement** applied on Windows, the row **composes**
  uncontained (`UNAVAILABLE_DIFF_COMPOSED_OK`).
- **Contained** with that same arrangement, it stops earlier at "private data
  root parent" — the ancestor-chain narrowing an AppContainer cannot perform,
  which is the half of the original reasoning that was right.

So neither of the two arrangements that exist today works on Windows in both
containment modes. **Windows needs a third**: a root created owner-private
*with the container's own entry*, which
`create_storage_root_owner_private` has provided since [[BPR-0833]].

The failure is also not about containment by itself: the benchmark fails
identically contained and uncontained, and two of the rows that share the
cause are registered `WINDOWS_UNCONTAINED`.

## Scope

- Give the Windows branch the third arrangement: create the data root
  owner-private with the container's entry, rather than either the bare
  `QTemporaryDir` or the POSIX narrowing.
- Verify it in **both** containment modes, since the two existing options each
  work in exactly one.
- Then reconsider whether the `#if` is still needed at all. If the third
  arrangement is correct everywhere, one path is better than two; if it is
  not, say on this card why each platform needs its own.
- `registry` is the catch-all arm of `github_application_startup_failure`, so
  the reported cause hides which profile-startup failure actually fired. The
  station needed a temporary print to find out. **Give that arm its own named
  failures** — it is the difference between one run and a bisect, and it is
  the same lesson as the cleanup cluster in [[BPR-0875]].

## Acceptance

- All five rows pass on Windows, contained and uncontained as each is
  registered.
- The data root arrangement is recorded here, with why Windows needs what it
  needs.
- `registry_unavailable` no longer stands in for several distinct startup
  failures, or the card says why it must.
- macOS and Linux keep passing all five.

## Two additions from the r5 critical review

**The sibling arm has the same disease, and a pin depends on this one.**

- The `unavailable` arm collapses `github_profile_runtime` construction,
  `start_services()` and `open_window` into one word
  (`src/app/src/github_application.cpp:12633-12655`), exactly as `registry`
  does for its seven causes. [[BPR-0882]] carries that arm and the two other
  boundaries that discard `what()`. Fix them in one pass if convenient; the
  work is the same shape.
- **`tests/end_to_end/CMakeLists.txt:2295-2296` pins
  `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`.** When this card splits
  the `registry` arm into named causes, that pin must move with it, or it will
  fail for the right reason and look like a regression. Change them together.

## Classified (dsp-006 scout): a genuine defect; containment is a red herring here

The containment rule is recorded once on [[BPR-0872]]. It does **not** settle
this card. Two of the five rows —
`application_review_ledger_composition_contract` and
`application_defect_composition_contract` — are already in
`WINDOWS_UNCONTAINED_TESTS`, so they run **outside** the container and fail
anyway, which matches this card's own observation that the benchmark fails
identically contained and uncontained.

So the work is the third data-root arrangement plus splitting the `registry`
arm (`src/app/src/github_application.cpp:12633-12655`), not a registration
change. Containment adds only one extra constraint, already established by
[[BPR-0833]]: the third arrangement must carry the container's own SID, which
`create_storage_root_owner_private` writes at creation.

**Name the arm before fixing it**, exactly as [[BPR-0875]] must name its three
cleanup sources: say which of the seven `registry`/`unavailable` sub-causes
fires. That change lands in `src/app/**`, which no station owns. Note also that
this card couples to the pin at `tests/end_to_end/CMakeLists.txt:2295-2296`,
which has to move in the same commit.
