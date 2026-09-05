---
schema: "implement-rfc.card.v1"
id: "BPR-0617"
title: "Verify and close RFC-BPR-0013"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0507","BPR-0616","BPR-0618","BPR-0619","BPR-0620"]
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
transaction kill recovery, Done). Allocate later
slices after inspecting retained product coverage; do not introduce a parallel
performance subsystem or fabricate timing budgets before measurement.

## Closeout checklist

| Criterion | Check / remaining work | Result and commit |
|---|---|---|
| Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content. | Inventory retained diff/workbench fixtures, then measure missing file-count/comment/epoch/mixed-content cases. | Open; no RFC-wide passing result claimed. |
| UI input, scrolling, and navigation remain responsive while background work runs. | Measure ordinary application input, navigation and scrolling while its selected workers are active. | Open; no RFC-wide passing result claimed. |
| Memory and cache usage remain within documented budgets and recover after closing/pruning reviews. | Measure process/cache retention through close and prune; justify hardware-specific thresholds before enforcing them. | Open; no RFC-wide passing result claimed. |
| Primary and secondary rate-limit simulations produce bounded backoff and useful UI state. | Reuse and assess provider/host rate-limit fixtures and their visible projection; add uncovered backoff cases. | Open; no RFC-wide passing result claimed. |
| Offline mode uses clearly labeled cached data and never implies freshness. | Inspect selected cached-data behavior and exercise explicit stale/offline presentation. | Open; no RFC-wide passing result claimed. |
| Process-kill tests recover Git cache and SQLite state without corruption or duplicate mutations. | BPR-0616: `live_review_submission_provider` and `live_review_submission_restart_recovery` passed 2/2, including five SIGKILL courses, SQLite integrity and zero duplicate POSTs. BPR-0618: all four real fetch/spool/prepared/visible kill courses pass, with repeated restart, exact epoch/ref identity, Git fsck, SQLite integrity and owned-spool cleanup. BPR-0619: all six receiving/spool/install transaction kills pass, including prior-history identity and two fresh restarts; unowned artifacts are preserved and new incoming admission is bounded. BPR-0620 adds forced owner PID/process-group death and fresh-launch evidence (`bdcd3837`); full AI-session workspace recovery remains to be scoped. | Partial passing evidence in BPR-0616 (`72022b52`) BPR-0618 (`081fc9d1`, `21255d9c`) and BPR-0619 (`62c62c6d`); full criterion remains open. |
| Fuzzers cover diff parsing/mapping, Markdown, settings/themes, API normalization, Git output parsing, and AI structured output. | Inventory custom parser entry points and add bounded fuzz targets/corpora for uncovered surfaces. | Open; no RFC-wide passing result claimed. |
| Logging and diagnostics remain metadata-only under large/error conditions. | Reuse basic Quill metadata-only and bounded-backpressure evidence (BPR-0602, BPR-0606); assess pathological workloads. | Open; no RFC-wide passing result claimed. |
| Architecture benchmarks and instrumentation prove active-set/changed-set scaling, serialized decision application, incremental Qt updates, deterministic cancellation, and stale-result rejection. | Reuse diff scale / host identity / incremental Qt evidence where inputs are unchanged; measure uncovered public paths. | Open; no RFC-wide passing result claimed. |

## Continuation

BPR-0616, BPR-0618 and BPR-0619 are complete. BPR-0620 is complete (`bdcd3837`): the real Unix launcher now stops its agent group after owner PID or process-group death, verifies both fixture processes are absent, and completes a fresh launch. All four focused local checks passed in 101.52s; the tests-off application build contains the expected Qt-free helper. This covers process lifetime, not full application workspace recovery.
Next, scope the complete Git/SQLite/workspace restart during an AI-session kill against the existing agent invocation host, protocol loop
and application composition fixtures. The inspected invocation-host tests cover
cooperative cancellation and a later run, but do not contain forced parent
process death; do not relabel that evidence as crash recovery. Use a local fake
agent process, never a paid/live agent invocation. Then inventory retained
public-path scale, memory, backoff/offline and parser-fuzz coverage before
allocating missing bounded slices. Reuse unchanged direct evidence from earlier
RFCs. No GitHub Actions, workflow dispatch, push, paid CI or live mutation is
part of this work. RFC-0013 and this closeout remain open.

### Next AI crash inspection

The existing `tests/end_to_end/agent_invocation_composition_public_path.cpp`
`positive-seed`/`positive-reopen` courses already select the ordinary application,
installed Git, SQLite session catalog and external canary. They preserve an
explicit caller-owned data root and verify session #1 plus next identity #2 on
reopen, but the seed exits through ordinary shutdown. Extend that composition
with a forced-death checkpoint while a stubborn canary runs, independent Git
fsck/SQLite integrity/catalog reads, and repeated ordinary reopens before a
second explicit invocation. Reuse the network-isolated runner and exact fixture
process cleanup from BPR-0620.

A separate recovery risk was identified by inspection: `github_application.cpp`
creates a fresh sibling `betterpullreview-agent-XXXXXX` workspace root each time,
disables generic QTemporaryDir removal, and retains exact native parent identity
only in memory for shutdown cleanup. A crash therefore cannot be assumed to
reclaim that root on restart. Observe and preserve the old root in the next
regression; do not add a wildcard sweep or acquire deletion authority merely
from its name. Scope any durable ownership/recovery fix from the observed
failure before expanding production changes. The SQLite session admission
transaction commits the session and sequence together; the next proof must
check retained identity and absence of automatic agent replay, not just a
successful new application start.
