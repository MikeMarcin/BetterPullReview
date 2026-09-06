---
schema: "implement-rfc.card.v1"
id: "BPR-0627"
title: "Diagnose and reduce measured large-review UI stalls"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0624"]
acceptance: [{"criterion":"Attributed main-thread work","outcome":"pass","revision":"d698b338fc21f8a0f31dadcf4410dffe0cab28d9","evidence":{"kind":"observation","detail":"Process-scoped macOS sampling identifies repeated conversation tree projection; temporary phase timers separately identify 164.10/163.25 ms native pane measurement and 111.14 ms semantic index preparation. Diagnostic probes removed; distinct initial-publication work assigned to BPR-0628/BPR-0629 before expanding implementation scope."}},{"criterion":"Selected-file correction and public regression","outcome":"pass","revision":"d698b338fc21f8a0f31dadcf4410dffe0cab28d9","evidence":{"kind":"observation","detail":"Core-projected file scope survives cleared/loading diff context. qt_workbench_window_scale_100, application_large_review_benchmark and application_unavailable_diff_public_path passed 3/3 in 42.16s; transient roots capped at two during 32 selections, 31 with outstanding work. Exact source, rendered window, navigation, idle wheel invariants and final selection preserved."}},{"criterion":"Measured local target and honest limitations","outcome":"pass","revision":"d698b338fc21f8a0f31dadcf4410dffe0cab28d9","evidence":{"kind":"observation","detail":"Uninstrumented rapid max interval decreased 201.09 to 44.34 ms and elapsed burst 12650.85 to 8262.84 ms on the same M4 Pro/Debug/Qt context. Provisional local maximum input-gap target 100 ms documented with roughly 2x observed headroom. Initial 338.75 ms pause, active-work scrolling and broader platform/budget criteria remain explicitly open on closeout and new cards."}},{"criterion":"Scope and bookkeeping","outcome":"pass","revision":"d698b338fc21f8a0f31dadcf4410dffe0cab28d9","evidence":{"kind":"observation","detail":"Documentation and git diff --check pass. Temporary profiling instrumentation removed exactly. No Actions, pushes, paid CI, credentials or production app launches."}}]
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

## Final diagnosis and bounded closeout

A temporary QElapsedTimer probe on the restored `d698b338` source separates the
initial large-file GUI work: semantic index 111.14 ms, native text install
62.12 ms, selection/paint 1.19 ms, left/right pane measurement 164.10/163.25 ms,
axis build/adoption 1.20/0.43 ms. The composed course passes under instrumentation;
these are diagnostic timings, not a replacement for the uninstrumented baseline.
The probe and its artifacts are removed.

As the acceptance requires, split these distinct remaining operations before
expanding ownership: BPR-0628 owns bounded cooperative native measurement and
BPR-0629 owns immutable semantic-index preparation off the UI publication turn.
The initial approximately 339 ms pause is not fixed by the selected-file scope
correction. Both follow-ups remain dependencies of BPR-0617, which stays open.

For rapid file switching on the recorded M4 Pro/Debug/Qt configuration, use a
provisional maximum event-loop interval target of 100 ms: it leaves roughly
2× headroom over the corrected 44.34 ms observation while rejecting the earlier
201.09 ms pauses. This is a local regression target, not an enforced portable CI
threshold or an RFC-wide responsiveness claim. The exact active-thread-set cap
is enforced in the composed test independently of wall-clock noise.

This card completes the measured file-switch correction, repeatable regression,
profile attribution, local target and explicit follow-up split. Final focused
checks and rendered/source evidence are recorded above; documentation and diff
checks pass. Native Windows/Linux evidence, active-work scrolling, the two
initial-publication corrections and full RFC-0013 budgets remain open. No
GitHub Actions, push, paid CI, credential operation or production app launch.
