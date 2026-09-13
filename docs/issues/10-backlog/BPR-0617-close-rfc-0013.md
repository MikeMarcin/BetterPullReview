---
schema: "implement-rfc.card.v1"
id: "BPR-0617"
title: "Verify and close RFC-BPR-0013"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0507","BPR-0616","BPR-0618","BPR-0619","BPR-0620","BPR-0621","BPR-0622","BPR-0623","BPR-0624","BPR-0626","BPR-0627","BPR-0628","BPR-0629","BPR-0630","BPR-0631","BPR-0632","BPR-0633","BPR-0634","BPR-0635"]
closeout_for: ["RFC-BPR-0013"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0013

## Current frontier — rfc0013-r1 / d1

The adopted [RFC-0013 milestone plan](../../rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json) binds `default-cache-byte-pressure` to BPR-0635 on local Mac, with the exact assignment and legacy-plan preparation receipt beside the plan. BPR-0612 and BPR-0634 are Done, so BPR-0635 is dependency-ready; its next public course must exceed the unchanged 64-MiB byte ceiling before 64 entries, prove exact eviction/revisit and owner release, then record native timing, input, memory, bounded work and close evidence. The current candidate starts at `21e7ae34`; no BPR-0635 source outcome is yet accepted. Preserve BPR-0622's Windows/Linux recovery gate, BPR-0643's BPR-0642 and owner-account dependencies, all remaining rate/offline/fuzz/diagnostic and live/human rows, and the canonical Proposed predecessors. No remote mutation, production account action, Actions, push or publication is part of this Mac course.

The first BPR-0635 composed attempt is a diagnostic failure, not byte-pressure evidence. The unchanged history course fails epoch revision preparation before selecting a large key, and unchanged loaded-close reaches 29 valid account reads but records replay `unexpected_operation` (code 9) on a subsequent request. The task logs under `build/bpr0635-` retain the exact attempt. The current bounded correction owns only the synthetic large-review replay and its direct/composed tests; the card stays In Progress until its original public byte and cleanup criteria pass. No production behavior change is inferred from this fixture failure.

The corrected 30-operation synthetic replay passes the original entry-pressure, loaded-close, resource and direct replay courses. Independent review found a historical fixture fetch-authority bypass and a weak stale-selection assertion; both are corrected in the current candidate. The historical fixture now rejects substituted or additional disconnected wants at the acquired transport's actual `fetch()` boundary with zero consumer delivery, before positive installed-Git fetch. The B-held public supersession observes source changes until all work drains, verifies exact B source/key, then revisits exact A. The gated byte course (`build/bpr0635-reviewed-byte-history.log`) passes in 363.08s: 70,353,760 cumulative admitted bytes at selection 20, first weak diff/index owners expired, peak 20 entries / 66,839,408 accounted bytes below the unchanged 64-MiB ceiling, eight B-held source observations, exact A revisit, 32 retained Git/catalog pairs, zero drained cache/jobs, and diff/index/window/documents released on destruction. Sampled physical-footprint growth is 127.782 MiB after drain and 38.453 MiB after destruction; byte-only 160-MiB drain and unchanged 96-MiB destruction limits pass. The corrected 32-epoch history regression reaches all 32 exact heads, 929 account reads and exact retained storage, but fails its pre-existing 100-ms Mac UI-interval budget: refresh pulses reach about 247 ms after the timeline/probe fixture changes. The same worker owns attribution and correction in the five fixture/benchmark/CMake paths; the failure is not waived. BPR-0635 remains In Progress until the history regression, source integration, documentation and acceptance complete. Earlier diagnostic and observational logs remain under `build/bpr0635-`; none indicates production data loss.

RFC base: `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`.

This is the first parent of `e61b156d5d11add32452c669550c47adaaddeac3`,
the earliest historical RFC-0013 crash slice recorded by BPR-0215. Existing
Done BPR-0215, BPR-0217 and BPR-0218 retain shutdown/liveness and temporary-state
fixes; later hygiene cards are not proof of the entire resilience RFC.

Requested set: remaining nondeferred RFC pack through RFC-BPR-0014, with
RFC-BPR-0008 the current closeout frontier and RFC-BPR-0013 before 0014.
The canonical index permits available-target implementation while earlier
live checks remain open. Closure still waits for BPR-0507 and every remaining
criterion below. Advanced logging remains deferred. Owner steering now puts the
full Glaze migration (BPR-0612) ahead of further performance work; completed
cache and lifetime coverage remains regression evidence, not a final parser baseline.

Planned cards: BPR-0616 (review-publication kill recovery, Done), BPR-0618
(Git-fetch/epoch kill recovery, Done), and BPR-0619 (live SQLite journal
transaction kill recovery, Done), BPR-0620 (agent parent-death cleanup, Done),
BPR-0621 (composed AI-session state recovery, Done), and BPR-0622 (durable
workspace reclamation, open). Allocate later slices after inspecting retained product coverage; do not introduce a parallel
performance subsystem or fabricate timing budgets before measurement.

## Closeout checklist

| Criterion | Check / remaining work | Result and commit |
|---|---|---|
| Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content. | BPR-0623/0624 supply the 515-file, 20,000-line, binary/generated and 1,000-comment workload. BPR-0631 independently checks all 31 stored head deltas; BPR-0632 drives all 32 application epochs, exact source and retained refs/catalog through public refresh/history/selection. | Fixture and Mac composed coverage pass in `75d1494e` (320.25s history replay), with 897 exact reads and 32 retained epochs. Broader latency/platform criteria remain separate. |
| UI input, scrolling, and navigation remain responsive while background work runs. | BPR-0627 corrects selected-file scope, BPR-0628 yields native measurement, and BPR-0629 moves semantic-index preparation off the GUI publication turn. BPR-0632 fixes pending single-anchor supersession and measures 31 rapid epoch changes, 30 during active work, with exact final source. | Partial passing Mac evidence: ordinary large-file maximum input interval 68.662 ms; final many-epoch maximum 71.247 ms and supersession 10.848 ms in `75d1494e`. One prior 112.448-ms source-selection sample missed the provisional 100-ms target and did not repeat unchanged; retain the outlier. Broader/native-platform acceptance remains open. |
| Memory and cache usage remain within documented budgets and recover after closing/pruning reviews. | BPR-0630 proves prune/delete cleanup, BPR-0632 bounds many-epoch caching, BPR-0633 proves independent loaded-window destruction, and BPR-0634 forces default 64-entry eviction/revisit plus cleanup through public selection. Default byte pressure remains required in BPR-0635, along with other-platform budgets. | Partial passing evidence through `470adea7`; entry peak 64 / 212,992 bytes, evicted raw/index release and exact revisit pass. Loaded-close footprint/reclamation and provisional 160/96-MiB budgets remain documented. No RFC-wide passing result claimed. |
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

BPR-0627 integrates the selected-file scope correction in
`d698b338`: clearing the old diff no longer repopulates the entire conversation
tree while the next file loads. The composed 32-selection course now caps the
transient root set at two relevant threads; its maximum timer interval falls
from 201.09 ms to 44.34 ms and elapsed burst from 12,650.85 ms to 8,262.84 ms.
Qt workbench, composed large-review and unavailable/recovery checks pass 3/3 in
42.16s; full-window render, documentation and diff checks pass. Initial large-file
selection still observes a 338.75 ms maximum interval, so no responsiveness
closeout or universal budget is claimed.

Next Mac work: attribute that remaining large-file pause, starting with measured
pane geometry/publication work identified in the retained profile observations,
and establish justified local targets through BPR-0628 and BPR-0629. Keep RFC-0013 open for remaining memory/pruning,
backoff/offline, fuzz/diagnostic, active-work scrolling and target-native
criteria. Preserve the Windows/Linux BPR-0622 handoff and earlier live gates.


Phase instrumentation now attributes that remaining initial pause: native pane
measurement takes 164.10/163.25 ms; semantic index preparation takes 111.14 ms;
text installation 62.12 ms; axis build/adoption 1.20/0.43 ms. These diagnostic
numbers split the remaining work into BPR-0628 (cooperative native measurement)
and BPR-0629 (index preparation outside the UI publication turn). The temporary
instrumentation is removed. BPR-0627's file-switch correction and diagnosis are
complete; the RFC responsiveness criterion remains open on these new cards and
broader active-work/platform coverage. The provisional target for rapid file
switch input on this recorded Mac is a maximum 100 ms event-loop interval,
roughly twice the corrected 44.34 ms observation; it is not a portable CI limit.


BPR-0628 completes cooperative native measurement in `49aefc1cf2c196168c280d0d7d3d9176251a72e9`
(helper `52a33b68`). Final focused axis/workbench/window/large-review checks pass
6/6 in 31.54s; the full-window raster is inspected. On the same recorded Mac,
94 measurement continuations admit 93 input callbacks; gaps between these
callbacks are maximum 5.356 ms / p95 4.488 ms. Each continuation has a hard
512-line cap and checks a 4 ms target between groups. Exact native geometry,
selection, cached scrolling, supersession and layout cancellation remain intact.
No incomplete axis can be adopted, and stale geometry input is suspended while
retaining the remeasurement anchor. No remote CI or live account was used.

The whole initial selection still records a 297.119 ms maximum interval
(2,329.20 ms total), distinct from the measured batching interval. Next Mac work
is BPR-0629: the existing host already owns the effect job, while both its finish
path and Qt publication repeat full-result validation/index preparation on the
UI decision lane. Bind a prepared immutable index to the exact accepted result,
include its retained allocation in cache accounting, and keep cancellation and
late-completion checks. Do not bypass validation or move Qt work to a worker.
Native text installation remains a separate measured limit. Rapid selection
remains below the provisional 100 ms local target (50.437 ms maximum here),
which is not a universal responsiveness claim. RFC-0013 remains open.


BPR-0629 completes prepared semantic-index publication in `fb55c7976715abb54dfda3ac65a551b1bbe05fa1`.
The existing effect job performs full validation/index construction once, then
host/Qt accept only the exact immutable index/result pair for the current
request. Cache hits reuse that index and its retained vector/object bytes fit
within unchanged cache ceilings. Missing/mismatched/stale Qt publication,
cache-budget refusal, reordered/cancelled/shutdown terminals and core/Qt
boundaries pass. Focused dev core/host checks pass 2/2 (0.95s), selected
Qt/application lanes 4/4 (41.75s), fixture/boundary/composition checks 5/5
(144.37s), and core-only semantic/boundary checks 3/3 (93.55s). The rendered
public source/navigation/scroll/selection course is preserved.

On the same recorded Mac, clean large-file maximum/p95 UI interval falls from
297.119 ms to 65.308/5.786 ms, and rapid selection is 28.196/5.679 ms maximum/p95.
A separate temporary timer attributes 64.730 ms to native text conversion and
installation; its code is removed and the clean benchmark rebuilt. The
provisional 100 ms same-Mac observation target covers that remaining measured
cost with roughly 50% headroom, without becoming a universal/native-platform
claim. The final two-result cache now accounts for both prepared indexes at
3,521,008 bytes. Native measurement still yields: 82 batches, 81 input
callbacks, maximum/p95 gap 5.236/4.209 ms.

Next inventory and exercise the remaining RFC-0013 memory/pruning, many-epoch,
backoff/offline, fuzz/diagnostic and broader active-work criteria, reusing earlier
relevant checks. The BPR-0622 Windows/Linux handoff and earlier live gates remain
open. All work remains local to this Mac; no remote CI or live account was used.


The next available Mac slice is BPR-0630, a direct large-review resource course.
Existing diff host ceilings and on-disk maintenance evidence remain relevant,
but neither shows native memory or immutable diff/index release after public
prune/delete/close. Measure those phases through the real composition, correct
demonstrated stale ownership, and establish local evidence-based thresholds.
Keep broader cache pressure, many-epoch and other native-platform claims open.


BPR-0630 completes the local maintenance resource slice in `316390ce13411ff1dfbae1ba3896256a62629a78`.
The new public course exposed retained stale source/diff/index after successful
prune. Accepted exact-epoch invalidation now releases those objects and their
cache while preserving other epoch selections and late-completion rejection.
Public prune/delete/window-close/destruction, exact Git/catalog consequences
and the before/pruned window renders pass. Final resource course is 14.32s;
large-selection footprint 86.69 MiB falls to 61.09 MiB after prune with zero
derived cache/jobs. The measured Mac-only sampled-growth/cleanup budgets are
160/96 MiB above process baseline; allocator/Qt retention is explicitly distinct
from object lifetime. Core/host/Qt/boundaries/isolation, original/unavailable
public courses, composition and documentation checks pass as recorded on the
card. This does not establish unsampled peaks, independent loaded-close memory,
cache-pressure or other platform budgets.

Next inventory existing evidence for many-epoch application replay, independent
loaded-close/cache-pressure, backoff/offline, fuzz/diagnostic and active-work
criteria; select the narrowest uncovered Mac slice. BPR-0622's Windows/Linux
work and earlier live gates remain open. No Actions, remote CI, push, paid work
or production account operation is authorized by this continuation.


The next many-epoch slices are BPR-0631 (compact independently checked per-head
input deltas and exact Glaze replay) and BPR-0632 (32 real composed refreshes,
historical/current selection, cancellation and measured Mac evidence). Existing
32 Git objects do not establish 32 application epochs. Preserve the original
latest-head workload and treat input/compiler evidence separately from the
required composed benchmark.


BPR-0631 completes exact earlier-head input/replay in `d0047177aa0305666b7ee811d445f3ea0b14621f`. A compact
14,569-byte sidecar is independently checked against every stored Git transition;
all 32 snapshots pass the production pager with complete 515-file/1,000-thread
counts and exact PR/commit/GraphQL/check/file identities. Original stored inputs
and the default latest-head course are preserved. Fixture, replay and latest-head
application checks pass (7.43s / 5.84s / 22.17s); negatives reject substituted or
missing epoch data. BPR-0632 is now dependency-ready for actual 32-epoch composed
refresh/selection/cancellation and measured evidence. Do not promote fixture-only
coverage to the whole many-epoch acceptance claim.


BPR-0632 completes the bounded 32-epoch application course in `75d1494e9b993b19ce2822fcdb7cc4d5023b837b`.
It exposes and fixes a real one-sided selection defect: while resolution was
pending, enabled selectors lost subsequent choices because the accepted pair
had been cleared. The core now uses the pending explicit pair, cancels the old
request and rejects stale results. The isolated test fails before the fix and
passes afterward; catalog/authority boundaries and existing host tests pass.
The fixture inspection getter also now uses the actual hexadecimal retained-ref
ordinal encoding beyond epoch 9; stored Git format is unchanged.

All 32 refreshed heads and retained catalog/ref pairs match independent stored
input. The final 31-choice public supersession course includes 30 changes during
active work, displays exact first/latest/previous-upload source and drains
shutdown. Both full windows are inspected. Final history replay passes 320.25s;
refresh/source UI maxima are 71.247/52.518 ms, and rapid epoch supersession is
10.848 ms maximum. The unchanged 100-ms target had one earlier 112.448-ms sample
at source selection 20; its cause is unproven and it did not recur on the single
unchanged repeat. Preserve this evidence and provisional status. Other four
focused application lanes, core/host, core-only/boundaries, network isolation,
builds and documentation checks pass as recorded on the card. No remote CI or
production account was used.

The next uncovered Mac slice is independent loaded-close memory: BPR-0630's
close occurred after pruning/deleting, and BPR-0632 does not sample loaded-close
memory. Reuse its native sampling and weak ownership observations in a fresh
public-window close course before claiming recovery for a still-loaded review.
Then address remaining cache pressure, backoff/offline, fuzz/diagnostic and
broader active-work criteria. BPR-0622's Windows/Linux work and earlier live
human gates remain open. No Actions, workflow dispatch, push or paid CI.

BPR-0633 owns the next independent loaded-review close course, preserving the
existing prune/delete resource lane and its measured provisional Mac budgets.


BPR-0633 completes independent loaded-window close evidence in `5590d3ff953bb3be29cf1445270048b201f15893`.
The large raw diff/index and 20,001-block native source documents remain live
when public close starts. Shutdown drains cache/jobs in 2.703 ms; subsequent
ordered application destruction releases the weak diff/index and watched
window/documents. Exact Git/catalog base/head identities survive with zero
prune/delete dispatches. A fixture-only retained comparison allows the existing
inspector to read after shutdown clears its live authority; product lifecycle
and storage policy are unchanged.

On the recorded Mac, sampled physical footprint is 92.095 MiB before close,
91.391 after drain and 55.595 after destruction (baseline 4.516). RSS retention
is reported separately; unchanged provisional 160/96-MiB growth/cleanup limits
pass. Loaded-close, legacy resources and composition pass 3/3 (14.66/14.29/53.33s),
as do network isolation, builds and documentation. The rendered loaded state is
inspected and task artifacts are removed. No Actions, remote CI or live account
operation occurred.

Next exercise composed default cache pressure beyond the 64-entry / 64-MiB
immutable diff/index limits, checking eviction, retained ownership and exact
source when revisiting an evicted selection. Existing host unit tests cover
small-capacity eviction and byte refusal; they do not establish the full native
application's default-capacity behavior. Reuse the stored large-review/epoch
inputs and direct public paths. Backoff/offline, fuzz/diagnostic, broader active
work, BPR-0622 Windows/Linux and earlier live/human gates remain open.

BPR-0634 and BPR-0635 separately exercise the default entry and byte ceilings
through real composed pressure. Both are required; small-capacity unit evidence
or many small entries does not establish the 64-MiB byte-pressure criterion.


BPR-0634 completes default entry pressure in `470adea7d7683577893b954f7a0c3d38c1f60689`.
The real public app selects 65 distinct small files at unchanged limits. Cache
entries stop at 64; the oldest weak raw diff/index survive through capacity and
expire at eviction. Revisiting the evicted file rematerializes its exact stored
blobs/source through the ordinary host. Peak bytes are only 212,992, explicitly
not byte-pressure evidence. Public close/drain/destruction empties cache/jobs,
releases current native/immutable owners and preserves Git/catalog data without
maintenance. Full-window source is inspected.

The same Mac records 54.600 ms maximum UI interval, 64 peak cache entries and
one diff/one workbench/two epoch jobs. Footprint is 52.954 MiB before close and
52.173 after destruction versus 4.547 baseline; retained Qt/allocator memory
and provisional 160/96-MiB growth budgets are explicit. Entry pressure, both
existing resource courses and isolation pass 4/4 (77.49/13.44/14.41/4.06s).
Build/documentation/diff checks pass; no production code or remote/account state
changed, and consumed task artifacts are removed.

Next is to split and implement BPR-0612, the full Qt JSON-to-Glaze migration,
before further performance work. After migration, BPR-0635 must use enough
distinct large comparisons across stored heads to exceed 64 MiB before reaching
64 entries. Verify byte eviction, weak ownership,
exact public revisit and cleanup, and measure native footprint for that larger
working set. Do not promote the small-file entry course to byte-budget proof.
Earlier live/human, Windows/Linux, backoff/offline, fuzz/diagnostic and broader
active-work criteria remain open; all execution remains local without Actions.
