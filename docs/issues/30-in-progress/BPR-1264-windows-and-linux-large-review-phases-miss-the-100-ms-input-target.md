---
schema: "implement-rfc.card.v1"
id: "BPR-1264"
title: "Large-review phases at or over the 100 ms input target (Windows, Linux, and one on macOS)"
status: "In Progress"
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

## Linux: attributed, repaired and gated — 2026-10-05 (`bpr-main-linux`)

**Reproduced** at `d7c828fa9` in the optimized tree (`app-release`, GCC
15.2 `-O3`, tests on, offscreen, otherwise idle), three runs each, largest
event-loop gap in ms: `large_file_selection` 83.8, 98.2, 104.8;
`large_source_revisit_render` 88.3, 87.7, 92.6; `approximate_file_selection`
163.4, 152.0, 133.8; `first_tab_file` 92.7, 103.3, 94.8.

**Owning cause** (portable). A temporary main-thread sampling profiler (1 ms,
`LD_PRELOAD`, not committed) put each phase's longest turn in
`diff_workbench::publish`, which replaced both panes' text in the publishing
turn. One `QPlainTextEdit::setPlainText` of the 20,000-line file is 25–38 ms
on this station, so the turn took 76–94 ms with both panes. In the approximate
course, `update_detail` also copied the whole opposing span, about the whole
file, into the compact detail pane (35 ms more, in the same turn). A standalone
Qt probe on the same Qt: one-pane `setPlainText` 34–37 ms; building a
detached document 28.5 ms, of which adopting it is 7–9 ms; 1,000-line appends
at most 3.2 ms each.

**Repair** (`src/adapters/qt`, `diff_workbench`):

- A publication with more than 4,096 lines on its two sides fills two detached
  documents, 256 lines per batch and at most about 4 ms per GUI turn.
- It then adopts them together with `setDocument`, carrying each pane's
  default font, text option (tab stop, wrapping) and margin. The rest of the
  publication runs unchanged. Nothing observes a partly filled document.
- A newer selection, a failure, an invalidation or a clear drops the staged
  documents, and `geometry_pending()` stays true until adoption.
- A smaller publication keeps the single synchronous replacement.
- The detail pane shows at most 64 lines of a span, then "(N more lines)".

**After** (same tree and conditions, three runs each): `large_file_selection`
25.3, 23.9, 26.8; `large_source_revisit_render` 24.6, 25.2, 23.4;
`approximate_file_selection` 36.6, 36.3, 36.4; `first_tab_file` 24.7,
24.6, 27.2. The whole phases also got shorter, for example approximate
selection 446–451 → 383–414 ms and revisit 328–330 → 292–300 ms.

**Gate.** Linux's `PlatformKnownInputMisses` is now empty. The 18
large-review rows (the 16, the fixture contract and the replay contract) pass
18/18 in 92.72 s with `BudgetsEnforced` true. The largest gap in any gated
Linux report is 51.1 ms (`first_tab_base`, which was not on the list).

**Controls:**

- `test_large_publication_fills_over_bounded_turns`
  (`qt_diff_workbench_scale_{100,125,200}`) pins staging, exact text, kept
  typography and supersession. With staging disabled it fails at "a large
  publication adopts nothing in the publishing turn".
- With the original `diff_workbench` and the empty list, the approximate
  course fails "fixture input target missed outside the platform's named
  phases" at 162.4 ms.

**Regressions** (Debug `build/dev`): the comparison surface's Qt, end-to-end
and large-review rows and `architecture_boundaries` pass 139 of 146. The 7
are the known [[BPR-1255]] six ("no review-bar control folds into an overflow
menu") and [[BPR-1236]]'s restoration row ("failed reading restoration
preserves exact recorded identities"). That row passed in an earlier run of
this change and failed before it, so it is intermittent, not this change.
`coding-format-check` passes.

**Owed:**

- **macOS:** confirmed and gated on 2026-10-05; see the next section.
- **Windows (the other half of this card):** the eight Windows phases are
  unmeasured after this change. `large_file_selection`,
  `large_source_revisit_render`, `approximate_file_selection` and
  `first_tab_file` share this cause. `shutdown`, `files_drawer_open`,
  `active_window_close` and `paged_review_and_git_load` were not attributed
  here, and Windows timer granularity ([[BPR-1259]]) is context for them. The
  card stays In Progress for them.

## macOS: confirmed and gated — 2026-10-05 (M4 Pro)

**Tree.** `ac0d9d1d4` in an optimized tree (`app-release` with tests on,
Apple clang 21 `-O3`, Qt 6.11.2, macOS 26.6.2, M4 Pro 48 GiB, otherwise
idle). Values are the largest event-loop gap in the phase, in ms, over the
responsiveness, launch, approximate and first-tab reports.

**Before** (the same tree with `diff_workbench` from `21cec606d` and the
macOS list empty; three runs of those four rows): `large_file_selection`
64.7, 62.8, 62.3; `large_source_revisit_render` 68.2, 67.3, 64.7;
`approximate_file_selection` 93.1, 95.3, 93.3; `first_tab_file` 65.2, 65.7,
64.4. The approximate phase stayed under 100 ms in these three runs, so it
did not fail them; the BPR-0967 runs measured it at 96.3–111.1.

**After** (`ac0d9d1d4`, three runs of the 18 rows): `large_file_selection`
18.9, 18.6, 18.6; `large_source_revisit_render` 14.0, 13.4, 12.2;
`approximate_file_selection` 18.7, 18.6, 18.9; `first_tab_file` 9.8, 10.9,
12.8. Across every report, including the resource courses,
`large_file_selection` reaches 22.0. The whole approximate phase went from
507–519 ms to 489–496 ms.

**Gate.** macOS's `PlatformKnownInputMisses` is now empty, so every macOS
phase gates the target. The 18 large-review rows (the 16, the fixture
contract and the replay contract) pass 18/18 in each of the three runs
(130.0, 130.3 and 131.6 s). Every report has `BudgetsEnforced` true, an
empty `KnownInputMisses` and no input or limit failure. The largest gap
outside the resource courses is 36.4–38.0 ms (`first_tab_base`, which was
not on the list).

**Workbench.** `qt_diff_workbench_scale_{100,125,200}`, including
`test_large_publication_fills_over_bounded_turns`, pass in the same tree.

Only the Windows half remains.
