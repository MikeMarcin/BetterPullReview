---
schema: "implement-rfc.card.v1"
id: "BPR-1043"
title: "Every epoch refresh in the large-review history course holds the GUI thread for about 167 ms against a 100 ms budget"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"bpr-main-mac, Mac batch 3, while verifying BPR-1032","found_at":"f97c08e6","related":["BPR-0967","BPR-1032","BPR-0893"]}
---

# BPR-1043: Every epoch refresh in the large-review history course holds the GUI thread for about 167 ms against a 100 ms budget

## Defect

`application_large_review_history` is red on macOS. It fails at
`large-review-benchmark-failed: epoch refresh exceeded the provisional
measured Mac budget` (`tests/end_to_end/large_review_benchmark.cpp`, the
check on `HistoryInputIntervalLimitMs` and `HistoryRefreshLimitMs`).

It was measured on an M4 Pro, Debug, Qt 6.11.2, with the row run alone at a
load average of 1.8–2.4. Two runs were made, one at `f97c08e6` itself and one
with [[BPR-1032]]'s measurement change applied
(`build/bpr1032-memory/{baseline,candidate}-cache/history-1.log`).

- **Every refresh stalls, not one.** All 31 `epoch_refresh_N` phases have a
  largest GUI-thread gap of about 167 ms: the median is 166.5 ms in one run
  and 167.4 ms in the other, and the worst single gaps are 173.0 and
  206.8 ms. The budget is 100 ms.
- **Duration is not the problem.** Every refresh completes in 8.2–8.4 s,
  inside its 12 s limit.
- **Source selection is fine.** The `epoch_source_selection_N` phases stay
  at about 30 ms.
- **It regressed.** The budget was set in `75d1494e` (2026-09-05), when the
  same course measured a 67.903 ms largest refresh gap. The comment beside
  the limit records that figure.
- **Not caused by BPR-1032.** The row fails identically without BPR-1032's
  change. The refreshes do not run that change's measurement code.

No open card recorded the row. It is registered only on macOS, inside
`if(APPLE)` in `tests/end_to_end/CMakeLists.txt`.

## Scope

- Find what an epoch refresh does on the GUI thread for about 167 ms at
  every refresh. Likely candidates are a model reset or a whole-list rebuild
  of the epoch line or selector as the catalogue grows to 32 heads, and a
  synchronous catalogue read. Measure it; do not guess.
- Find when it regressed from 68 ms. The recorded runs of this row and the
  Qt 6.11.2 upgrade (`92c4e159`) are the first places to look.
- Fix it at its cause. If some of the work has to stay on the GUI thread,
  bound it and batch it the way the pane measurement is batched.
- Do not loosen the 100 ms budget to pass. A budget change needs its own
  measured justification, as [[BPR-0967]] AC2 says.

## Acceptance

- `application_large_review_history` passes on macOS, with every refresh's
  largest GUI gap inside the 100 ms budget.
- The card names the cause and the commit, or the window, where the gap
  grew from 68 ms.
- A focused check fails on the pre-fix behaviour.
