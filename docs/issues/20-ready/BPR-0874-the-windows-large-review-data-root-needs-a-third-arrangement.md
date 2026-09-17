---
schema: "implement-rfc.card.v1"
id: "BPR-0874"
title: "Windows: five rows refuse at startup with registry_unavailable off the benchmark's bare temporary data root"
status: "Ready"
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
