---
schema: "implement-rfc.card.v1"
id: "BPR-0617"
title: "Verify and close RFC-BPR-0013"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0507","BPR-0616","BPR-0618","BPR-0619","BPR-0620","BPR-0621","BPR-0622","BPR-0623","BPR-0624","BPR-0626","BPR-0627"]
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
| Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content. | BPR-0623 supplies a synthetic 515-file pack with a 20,000-line comparison, binary/generated content, 32 heads and 1,000 paged comments. Stored-object and semantic-drift checks pass. BPR-0624 serves the latest head through the ordinary provider/installed-Git/application path and measures it. | Input boundary passes in `ee1717c2`; latest-head composed course passes in `e9da7abc` (27.30s). Many-epoch application replay and justified budgets remain open. |
| UI input, scrolling, and navigation remain responsive while background work runs. | BPR-0624 verifies 32 selections (31 with outstanding diff work), exact final source, change navigation and idle wheel invariants. Its Mac timer observes 387.48 ms/201.09 ms maximum intervals in large/rapid selection. BPR-0627 owns profiling/correction and active-work input follow-up. | Open; correctness passes in `e9da7abc`, but measured stalls prevent an RFC-wide responsiveness claim. |
| Memory and cache usage remain within documented budgets and recover after closing/pruning reviews. | Measure process/cache retention through close and prune; justify hardware-specific thresholds before enforcing them. | Open; no RFC-wide passing result claimed. |
| Primary and secondary rate-limit simulations produce bounded backoff and useful UI state. | Reuse and assess provider/host rate-limit fixtures and their visible projection; add uncovered backoff cases. | Open; no RFC-wide passing result claimed. |
| Offline mode uses clearly labeled cached data and never implies freshness. | Inspect selected cached-data behavior and exercise explicit stale/offline presentation. | Open; no RFC-wide passing result claimed. |
| Process-kill tests recover Git cache and SQLite state without corruption or duplicate mutations. | BPR-0616: `live_review_submission_provider` and `live_review_submission_restart_recovery` passed 2/2, including five SIGKILL courses, SQLite integrity and zero duplicate POSTs. BPR-0618: all four real fetch/spool/prepared/visible kill courses pass, with repeated restart, exact epoch/ref identity, Git fsck, SQLite integrity and owned-spool cleanup. BPR-0619: all six receiving/spool/install transaction kills pass, including prior-history identity and two fresh restarts; unowned artifacts are preserved and new incoming admission is bounded. BPR-0620 adds forced owner PID/process-group death and fresh-launch evidence (`bdcd3837`); BPR-0621 exercises the real application during an AI-session kill, independent Git/SQLite reads, two no-replay reopens and explicit session #2 (`430b7c51`, 43.68s). BPR-0622 now integrates exact Unix workspace reclamation and guarded Git materialization (`0427dc76`), including live-owner/substitution negatives and preserved Git/SQLite identity. | Partial passing evidence in BPR-0616 (`72022b52`) BPR-0618 (`081fc9d1`, `21255d9c`) BPR-0619 (`62c62c6d`) and BPR-0621 (`430b7c51`); Mac state/workspace recovery lanes pass, including BPR-0622 (`0427dc76`); non-Unix integration and other target-native evidence remain open. |
| Fuzzers cover diff parsing/mapping, Markdown, settings/themes, API normalization, Git output parsing, and AI structured output. | Inventory custom parser entry points and add bounded fuzz targets/corpora for uncovered surfaces. | Open; no RFC-wide passing result claimed. |
| Logging and diagnostics remain metadata-only under large/error conditions. | Reuse basic Quill metadata-only and bounded-backpressure evidence (BPR-0602, BPR-0606); assess pathological workloads. | Open; no RFC-wide passing result claimed. |
| Architecture benchmarks and instrumentation prove active-set/changed-set scaling, serialized decision application, incremental Qt updates, deterministic cancellation, and stale-result rejection. | Reuse diff scale / host identity / incremental Qt evidence where inputs are unchanged; measure uncovered public paths. | Open; no RFC-wide passing result claimed. |

## Continuation

BPR-0616, BPR-0618, BPR-0619, BPR-0620 and BPR-0621 are complete. BPR-0622
remains In Progress with its Unix production integration committed in
`0427dc76`. The application now retains durable exact ownership through Git
materialization and agent execution and recovers it on ordinary startup through
the effect lane. Unknown/substituted ownership is preserved with a visible
repair/refusal. The SQLite session catalog format is unchanged.

Its final materialization crash/cancellation course passes in 45.66s and the
running-agent application crash course passes in 43.41s. Both independently
check exact Git/SQLite state; the latter adds two no-replay reopens and explicit
session #2. Restart excludes a paused live supervisor, then reclaims its
workspace after child teardown. This supersedes BPR-0621's preserved-orphan
observation where durable ownership exists. Original preparation/cleanup
journal crash evidence remains in BPR-0622.

Independent review caught unsupervised materialization Git in the first wiring;
all workspace Git commands now retain a guard through the bundled supervisor.
The new test also exposed Darwin reporting CLD_STOPPED despite WEXITED. The
supervisor now distinguishes stopped children from actual exit events. Final
launcher, parent-death and materialization checks pass 3/3 in 142.86s. The normal
composition/cancellation/cleanup/diagnostic lanes, Git contract, native journal,
host, architecture, isolation manifest, dev/tests-off builds and documentation
checks pass as recorded with precise revision relevance on BPR-0622.

Next implement the remaining non-Unix journal/lifetime integration and obtain
required target-native evidence. No Windows or Linux passing result is inferred
from the Mac. BPR-0622 and this closeout stay open. Separately inventory retained
public-path scale, memory, backoff/offline and parser-fuzz coverage before
allocating missing bounded slices; reuse unchanged direct evidence from earlier
RFCs. No GitHub Actions, workflow dispatch, push, paid CI or live mutation is
part of this work. Production sign-in is preserved.


BPR-0623 completes the deterministic large-review workload fixture boundary
in `ee1717c2`. The existing 2,000-line workbench and paged epoch model evidence is
retained, but the two/five-file served PRs do not cover the RFC-wide workload.
This slice supplies independently verified inputs; a following application
benchmark slice must establish selected-path loading, interaction measurements
and justified platform budgets. BPR-0622's remaining Windows/Linux work is now
explicitly platform-filtered and remains open with its native handoff.


BPR-0624's composed latest-head course now passes in `e9da7abc`, including all
29 paged account requests, 515 files, 1,000 exact source threads/comments,
installed-Git blob identity, both 20,000-line displayed documents, full-window
render, change actions, idle wheel invariants and rapid-selection precedence.
BPR-0625's core fix `10520a43` extends the unchanged matrix ceiling with an exact
ordered unique-line route, separately bounded by actual index/comparison work.
Independent review's undercount finding is fixed; small reference equivalence,
normalization, refusal and exact/exhausted limits pass in dev and core-only.

BPR-0624 retains hardware and measured phase details. On this M4 Pro/48 GiB,
macOS 26.6.2 arm64, Debug Qt 6.10.3 offscreen/Fusion Mac, the composed course
passes in 27.30s. Maximum timer intervals of 387.48 ms during large selection
and 201.09 ms during rapid selection are material remaining work, not passing
responsiveness budgets. Final diff cache is two entries/3,517,512 bytes, which
is not a process-memory or pruning proof. Only the latest application epoch
is exercised. Core-only diff/architecture checks pass 4/4; dev architecture,
fixture, isolation, retained single-page application and composition checks
pass 6/6. Documentation and diff checks pass after recording the completed core
dependency. Source changes are local; no paid/remote CI was used.

BPR-0626 now presents typed unavailable/binary comparisons truthfully in
`945043fd`, retaining selectable wording and genuine equal-text counts. The
real composed compute-limit/binary/supported-recovery course passes (15.72s),
with its full-window render inspected. Qt, default large-review, isolation and
composition checks pass 5/5 in 98.80s; documentation/diff checks pass. Production
limits and remembered sign-in are unchanged.

Next Mac slice: BPR-0627 profiles and reduces the measured main-thread stalls
before selecting justified local targets. Keep RFC-0013 open for remaining memory/pruning,
backoff/offline, fuzz/diagnostic, active-work scrolling and target-native
criteria. Preserve the Windows/Linux BPR-0622 handoff and earlier live gates.
