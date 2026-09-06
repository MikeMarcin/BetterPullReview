---
schema: "implement-rfc.card.v1"
id: "BPR-0617"
title: "Verify and close RFC-BPR-0013"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0507","BPR-0616","BPR-0618","BPR-0619","BPR-0620","BPR-0621","BPR-0622"]
closeout_for: ["RFC-BPR-0013"]
---

# Verify and close RFC-BPR-0013

RFC base: `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`.

This is the first parent of `e61b156d5d11add32452c669550c47adaaddeac3`,
the earliest historical RFC-0013 crash slice recorded by BPR-0215. Existing
Done BPR-0215, BPR-0217 and BPR-0218 retain shutdown/liveness and temporary-state
fixes; later hygiene cards are not proof of the entire resilience RFC.

Requested set: remaining nondeferred RFC pack through RFC-BPR-0014, with
RFC-BPR-0008 the current closeout frontier and RFC-BPR-0013 before 0014.
The canonical index permits available-target implementation while earlier
live checks remain open. Closure still waits for BPR-0507 and every remaining
criterion below. Advanced logging and broad Glaze conversion remain deferred.

Planned cards: BPR-0616 (review-publication kill recovery, Done), BPR-0618
(Git-fetch/epoch kill recovery, Done), and BPR-0619 (live SQLite journal
transaction kill recovery, Done), BPR-0620 (agent parent-death cleanup, Done),
BPR-0621 (composed AI-session state recovery, Done), and BPR-0622 (durable
workspace reclamation, open). Allocate later slices after inspecting retained product coverage; do not introduce a parallel
performance subsystem or fabricate timing budgets before measurement.

## Closeout checklist

| Criterion | Check / remaining work | Result and commit |
|---|---|---|
| Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content. | Inventory retained diff/workbench fixtures, then measure missing file-count/comment/epoch/mixed-content cases. | Open; no RFC-wide passing result claimed. |
| UI input, scrolling, and navigation remain responsive while background work runs. | Measure ordinary application input, navigation and scrolling while its selected workers are active. | Open; no RFC-wide passing result claimed. |
| Memory and cache usage remain within documented budgets and recover after closing/pruning reviews. | Measure process/cache retention through close and prune; justify hardware-specific thresholds before enforcing them. | Open; no RFC-wide passing result claimed. |
| Primary and secondary rate-limit simulations produce bounded backoff and useful UI state. | Reuse and assess provider/host rate-limit fixtures and their visible projection; add uncovered backoff cases. | Open; no RFC-wide passing result claimed. |
| Offline mode uses clearly labeled cached data and never implies freshness. | Inspect selected cached-data behavior and exercise explicit stale/offline presentation. | Open; no RFC-wide passing result claimed. |
| Process-kill tests recover Git cache and SQLite state without corruption or duplicate mutations. | BPR-0616: `live_review_submission_provider` and `live_review_submission_restart_recovery` passed 2/2, including five SIGKILL courses, SQLite integrity and zero duplicate POSTs. BPR-0618: all four real fetch/spool/prepared/visible kill courses pass, with repeated restart, exact epoch/ref identity, Git fsck, SQLite integrity and owned-spool cleanup. BPR-0619: all six receiving/spool/install transaction kills pass, including prior-history identity and two fresh restarts; unowned artifacts are preserved and new incoming admission is bounded. BPR-0620 adds forced owner PID/process-group death and fresh-launch evidence (`bdcd3837`); BPR-0621 exercises the real application during an AI-session kill, independent Git/SQLite reads, two no-replay reopens and explicit session #2 (`430b7c51`, 43.68s). Its orphaned workspace remains for BPR-0622. | Partial passing evidence in BPR-0616 (`72022b52`) BPR-0618 (`081fc9d1`, `21255d9c`) BPR-0619 (`62c62c6d`) and BPR-0621 (`430b7c51`); Mac state-recovery lanes pass, while complete resource recovery and other target-native evidence remain open. |
| Fuzzers cover diff parsing/mapping, Markdown, settings/themes, API normalization, Git output parsing, and AI structured output. | Inventory custom parser entry points and add bounded fuzz targets/corpora for uncovered surfaces. | Open; no RFC-wide passing result claimed. |
| Logging and diagnostics remain metadata-only under large/error conditions. | Reuse basic Quill metadata-only and bounded-backpressure evidence (BPR-0602, BPR-0606); assess pathological workloads. | Open; no RFC-wide passing result claimed. |
| Architecture benchmarks and instrumentation prove active-set/changed-set scaling, serialized decision application, incremental Qt updates, deterministic cancellation, and stale-result rejection. | Reuse diff scale / host identity / incremental Qt evidence where inputs are unchanged; measure uncovered public paths. | Open; no RFC-wide passing result claimed. |

## Continuation

BPR-0616, BPR-0618, BPR-0619, BPR-0620 and BPR-0621 are complete. The Mac
application's AI-session kill proof (`430b7c51`) passes in 43.68s, including
Git fsck/ref identity, SQLite integrity/session identity, two no-replay reopens
and explicit session #2 with new-workspace cleanup. The interrupted workspace
retains 18 exact entries; complete resource recovery is still false.

BPR-0622 is In Progress. Its Unix lifetime prerequisite (`e50edf77`) passes
paused-supervisor lock exclusion after application PID/group death (25.15s),
normal protocol/architecture checks (79.46s) and the tests-off application build.
No production caller supplies the guard yet; this does not reclaim workspaces.

Its Unix journal prerequisite is now implemented in `77651b10`: bounded
append-only Glaze records retain original native identities, exact scope and
cleanup phases, and also provide the shared lifetime lock. The final dedicated
native test passes in 3.34s with fourteen SIGKILL courses, repeated recovery,
live-owner exclusion, substitution/corruption refusal and bounded admission.
The existing Git public contract passed in 84.24s; architecture and isolation
manifest checks also pass. BPR-0622 records the precise evidence and limits.

Next integrate that API with installed-Git/cache effects, the workspace host
and application construction/restart/shutdown. The production QTemporaryDir
and child cleanup capabilities are still in memory; no caller uses the new
journal or supplies its guard to launch. Materialize into the exact journaled
child, retain the owner through agent teardown and recover on ordinary startup.
Keep the SQLite session catalog unchanged. Unknown names can demand repair but
never establish deletion authority. Upgrade the composed kill course from
orphan preservation to actual reclamation, two no-replay reopens and explicit
session #2 with unchanged Git/SQLite identity. Non-Unix registry/lifetime
integration and target-native evidence remain open.

Then inventory retained public-path scale, memory, backoff/offline and
parser-fuzz coverage before allocating missing bounded slices. Reuse unchanged
direct evidence from earlier RFCs. No GitHub Actions, workflow dispatch, push,
paid CI or live mutation is part of this work. RFC-0013 remains open.
