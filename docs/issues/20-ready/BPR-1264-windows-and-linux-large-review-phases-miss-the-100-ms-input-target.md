---
schema: "implement-rfc.card.v1"
id: "BPR-1264"
title: "Large-review phases at or over the 100 ms input target (Windows, Linux, and one on macOS)"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-0967 station runs, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1264--BPR-1265--e0e97a5e10af.json","related_cards":["BPR-0967","BPR-1043","BPR-1259"]}
---

# BPR-1264: Windows and Linux large-review phases at or over the 100 ms input target

## Observation

[[BPR-0967]]'s optimized station runs (Windows at `682ea1392`, Linux at
`e257b236a`, both on an i9-10900KF; summaries in
[`docs/verification/BPR-0967-station/`](../../verification/BPR-0967-station))
measured these phases at or over 90 ms of the unchanged 100 ms input target in
the courses that gate it (responsiveness, launch, active work, pathological,
provider cancellation). Values are the largest event-loop gap in the phase,
or the whole phase for `files_drawer_open`:

| Platform | Phase | Observed ms |
|---|---|---|
| Windows | `large_file_selection` | 94.1, 105.2, 130.5 |
| Windows | `large_source_revisit_render` | 105.8, 106.5 |
| Windows | `shutdown` | 93.4, 105.8 |
| Windows | `files_drawer_open` (whole phase) | 98.2 |
| Windows | `active_window_close` | 93.2 |
| Windows | `paged_review_and_git_load` | 98.2 |
| Linux | `large_file_selection` | 97.2, 99.8, 100.2 |
| Linux | `large_source_revisit_render` | 92.2 |

The Mac measures the same phases at 26-74 ms. BPR-0967 names these phases in
the benchmark's `PlatformKnownInputMisses` for each platform, so they are
reported (`Extended.KnownInputMisses`, `InputBudgetFailures` when over) and not
enforced; every other phase gates at 100 ms. The target was not raised.

## Outcome

Attribute each named phase on its platform with exact public-course
observations, repair the owning cause where justified (failing-before control,
affected composed courses), and remove the phase from that platform's list once
its optimized runs stay under 100 ms. A phase that cannot meet the target keeps
its diagnosis here. Windows timer granularity ([[BPR-1259]]) is context to
check, not an attribution.

## Direct acceptance

- Each listed phase is either removed from its platform's list with an
  optimized station run under 100 ms, or carries a recorded owning cause and
  owner decision.

## Confirmation run and newly gated courses — 2026-10-01

The confirmation runs at `f61340e17` (`windows-confirm-*` and `linux-confirm-*`
summaries) widen the observed ranges: Windows `large_source_revisit_render`
105.0-115.4, `shutdown` 93.4-124.9, `active_window_close` 93.2-104.0; Linux
`large_file_selection` 94.4-108.0, `large_source_revisit_render` 90.4-115.3.

From `9aa3df3bb` every course without the heap sampler gates the target, not
only the responsiveness, active-work, pathological and provider courses. That
names three more phases:

| Platform | Phase | Observed ms |
|---|---|---|
| macOS | `approximate_file_selection` | 96.3, 97.6, 99.4, 111.1 |
| Windows | `approximate_file_selection` | 153.7, 179.9 |
| Windows | `first_tab_file` | 122.4, 154.1 |
| Linux | `approximate_file_selection` | 117.4, 127.6 |
| Linux | `first_tab_file` | 99.8 |

`approximate_file_selection` is the work-limited approximate alignment of the
20,000-line file; `first_tab_file` opens the first comparison tab's file. The
four resource courses stay ungated: their 1 ms heap sampler perturbs the thread
it measures.
