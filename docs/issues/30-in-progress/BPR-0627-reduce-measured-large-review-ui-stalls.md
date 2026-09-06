---
schema: "implement-rfc.card.v1"
id: "BPR-0627"
title: "Diagnose and reduce measured large-review UI stalls"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0624"]
---

# Diagnose and reduce measured large-review UI stalls

The BPR-0624 composed latest-head course passes source/interaction correctness,
but its 5 ms event-loop timer observed a 387.48 ms maximum interval during
20,000-line selection and 201.09 ms during rapid file selection. These are
measured delays, not an accepted responsiveness budget or a diagnosis of a
particular function. The idle 1,000-event wheel batch took 318.03 ms; it does
not establish scrolling responsiveness while background work runs.

Context: Apple M4 Pro, 48 GiB RAM, 14 logical processors, macOS 26.6.2 arm64,
Debug, Qt 6.10.3 Fusion/offscreen, 1600×1000 at DPR 1. Inputs: stored
`large-review-v1`, 515 files, 1,000 source threads, one current application
epoch. Final source matched after all 32 selections; 31 were issued with
outstanding diff work. A 3,517,512-byte/two-entry diff cache is an observation,
not evidence for process memory or pruning ceilings.

## Bounded acceptance

- Attribute the longest main-thread intervals to concrete work through focused
  local profiling/instrumentation; preserve background/core/Qt ownership.
- Implement the smallest justified correction to the measured blocking work.
  Preserve exact source, semantic identity, final-selection precedence and
  existing incremental scrolling invariants.
- Repeat the same selected composed course after the correction on a quiet Mac
  and compare elapsed phases, maximum/p95 intervals and work-overlap counters.
  Exercise input during active work explicitly; distinguish it from idle input.
- Document a justified local target and any remaining stalls. Do not declare a
  universal threshold from one machine or infer native Windows/Linux evidence.
  If profiling identifies separate work, split it before expanding ownership.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work stays local and uses synthetic fixtures. No GitHub Actions, push,
paid CI, credential use or production application launch.

## Profile-directed implementation boundary

A 24-second, 1 ms macOS `sample` attached only to the network-denied composed
benchmark descendant on source `945043fd`. Sampling is diagnostic, not a timing
baseline. In the rapid-selection call subtree, 4,140 main-thread samples run
through conversation endpoint placement; 3,307 run through an individual
conversation-model `dataChanged` emission and downstream tree layout. The model
currently emits each changed sibling row separately even when adjacent rows
change together. The panel republishes anchor availability as selection changes.

The integrated correction in `d698b338` owns the Qt workbench's conversation
filter and focused tests. Clearing a diff used to clear its file filter, briefly
showing the whole PR before the next file's diff arrived. The filter now follows
the core-projected selected file throughout that asynchronous gap; standalone
diff presentation retains its context fallback. An explicit absent core file
selection restores the whole-review view. No endpoint-authority, model-row,
thread-content, selection-policy or deferred-layout rule changes.

Two discarded experiments (batching row notifications and deferring layout for
hidden views) passed their correctness checks but did not fix the composed
course: rapid maximum intervals remained around 191–199 ms. Those source/test
changes were removed. The conversation tree was being repopulated with unrelated
threads during the selection gap; a hidden-view optimization did not address it.

## Integrated partial result

Three final local dev checks pass 3/3 in 42.16s on `d698b338`:
`qt_workbench_window_scale_100` (2.16s),
`application_large_review_benchmark` (22.99s), and
`application_unavailable_diff_public_path` (16.99s).
The focused Qt regression covers selection before a diff, the cleared-diff gap,
next publication and explicit removal of core selection. The composed course
now asserts that all 32 rapid selections never transiently expose more than two
relevant root threads. The measured maximum is two. Exact 20,000-line source,
change navigation, idle wheel invariants, final selection and clean shutdown
still pass; the full-window render was inspected. All 31 overlapping inputs
still occurred with outstanding diff work.

Same Mac/Debug/Qt/scale context as the baseline, no profiler or concurrent build
in the final run:

| Phase | Original elapsed / max interval ms | Current elapsed / max interval ms | Current p95 ms |
|---|---:|---:|---:|
| Large-file selection | 2,237.70 / 387.48 | 1,957.09 / 338.75 | 5.70 |
| Rapid selection | 12,650.85 / 201.09 | 8,262.84 / 44.34 | 5.75 |

Current load: 10,945.30 ms (maximum timer interval 48.18 ms); idle 1,000-event
wheel batch: 327.31 ms; final supersession drain: 996.18 ms. Diff cache remains
two entries/3,517,512 bytes. These are observations, not memory/pruning ceilings.
Documentation and diff checks pass. Profile/build/test logs and generated
JSON/PNG artifacts were consumed into this record and removed.

## Remaining acceptance

Keep this card In Progress. The 338.75 ms initial large-file timer interval
remains material; a local threshold is not yet justified/enforced for that
path. The original profile also contains 342 main-thread samples under
`diff_workbench::rebuild_geometry`; attribute the remaining pause on the current
candidate before choosing a fix. Inspect measured-pane geometry and publication
work while preserving exact native source geometry, cancellation and stale-result
rejection. Do not assume uniform line heights or publish partial axes merely to
meet a timing target. The selected-file regression is complete, but the card's
remaining profiling/budget criteria and RFC-wide active-work scrolling remain
open. All work stays on the Mac with synthetic fixtures; no Actions, push,
paid CI, credential operation or production application launch.
