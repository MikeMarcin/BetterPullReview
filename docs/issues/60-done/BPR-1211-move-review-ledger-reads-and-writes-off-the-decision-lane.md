---
schema: "implement-rfc.card.v1"
id: "BPR-1211"
title: "Move review-ledger reads and writes off the decision lane"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_card":"BPR-0972","disposition":"architectural responsiveness gap identified during structural scaling inventory; no latency or whole-architecture acceptance supplied"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9ad03a828","evidence":{"kind":"observation","detail":"Every review-ledger store operation (open, receipt submit/observe/refresh/load, conversation load and Mark read, private notes/defects/head, agent sessions, promoted agent notes) runs on the profile ReviewLedgerEffects lane through a FIFO host with at most 64 outstanding operations; decisions and typed completions stay on the decision lane; no lock is held across storage and reads answer only from loaded or committed state, with each miss an explicit async load. Call-site inventory is on the card; rg finds no remaining synchronous store read."}},{"criterion":"AC2","outcome":"pass","revision":"9ad03a828","evidence":{"kind":"command","detail":"application_review_ledger_host: durable-before-publication ordering, FIFO rapid gestures with single-failure rollback, material supersession, retirement cancels queued work and refuses a late completion, store closed on the effect lane, same-root recovery, invalidation-safe reads. Composition guards answers by window lifetime and ledger generation. application_agent_invocation_host and application_private_review_host pass with the async session port. application_profile_multi_window_public_path passes sibling receipt, conversation and defect fanout plus PR replacement."}},{"criterion":"AC3","outcome":"pass","revision":"9ad03a828","evidence":{"kind":"command","detail":"Held-store host course (real worker thread: lane free, reads immediate, admission bounded, nothing published before release) plus inline-effect negative control (1505 ms vs 0 ms); composed held-ledger step in application_active_changed_scaling (lane turn 1 ms, no completion while held) plus composed inline-effect negative control failing; negative control removing late-completion refusal fails. Composed receipt, conversation, cold-cache and sibling courses pass; full dev suite has the same 43 failures at baseline d25f17b69 and at this candidate, with matching failure signatures. macOS arm64 only; Windows/Linux not run; timings are observations, not budgets."}}]
---

# Move review-ledger reads and writes off the decision lane

BPR-0972 measures data visited by routine public gestures and records the existing
ledger's synchronous I/O. A bounded one-row read or write is not proof that the
serialized decision lane remains responsive during SQLite contention or slow
storage. This card retains that separate architectural gap. It does not waive the
application's effect/decision-lane boundary, infer Windows/Linux native evidence,
or replace BPR-0967's optimized hardware latency and memory budgets.

## Observed ownership and calls

`src/app/src/github_application.cpp` composes a window-owned
`review_ledger_application_host` and store in `compose_review_ledger`. Its
`dispatch_receipt` calls `submit` synchronously; `observe_selected_material` calls
`observe_material`; `refresh_shared_ledger` calls `refresh_receipt` after sibling
invalidation. Conversation Mark read calls `mark_conversation_read`. Accepted
Overview and private-context publication call `load_receipt`,
`private_observation` and `observe_conversations`. `retire_review_ledger` destroys
this host and store at authority replacement. Existing exact authority and
file/material guards must survive any scheduling change.

`src/application/src/review_ledger_application_host.cpp` currently calls the store
under its mutex: `observe_material` loads one receipt and may commit a core-decided
carry; `submit` commits one receipt; `refresh_receipt` loads one receipt;
`mark_conversation_read` writes one cursor. `load_receipt` and conversation receipt
reads have retained caches. `private_observation` directly loads the bounded
private-state observation; private note/defect/head/session commands also perform
synchronous store operations. Moving a whole locked method to a worker while the
GUI waits for that mutex would leave the responsiveness gap intact.

The BPR-0972 safe composed course has 515 files, 1,000 foreign-authored threads and
20,000 source lines. Selecting one file loads one receipt; Seen and Mark file read
each issue one persistence request and no receipt read; conversation Mark read
writes one conversation receipt and visits one thread/post. Exact per-gesture
private/conversation load counters and allocation limits are recorded in the
[BPR-0972 evidence](../../verification/bpr0972-active-changed-scaling.md).
These bounded counts say nothing about storage waiting time.

## Acceptance

- AC1: Move admitted ledger I/O off the decision lane with bounded outstanding work. Keep deterministic transitions and immutable typed completion on the serialized lane, and prevent GUI reads from waiting on a mutex held across I/O. Inventory startup, receipt, conversation and private-state calls so a cache hit cannot hide an unconverted miss or invalidation path.
- AC2: Preserve durable-before-publication and FIFO semantics for rapid gestures, exact file/material and profile/account/PR authority, and the lifetime of stores through retirement. A closed/replaced window or stale completion cannot publish into its successor. Sibling invalidation remains typed and changed-data bounded.
- AC3: Hold a fake store operation while independently advancing the decision lane; prove responsiveness, bounded admission, cancellation/late completion refusal, failure rollback, shutdown/drain and same-root recovery. Exercise the real composed receipt/conversation entry points and cold-cache/sibling paths, with direct negative controls for an inline effect or omitted authority check. Report native/platform limits explicitly.

## Continuation

Use safe local fixtures. The reserved allocation receipt is
`docs/issues/_reservations/BPR-1211--BPR-1211--eab9cb4396e7.json`.
No live account, private target, remote mutation, release or push is authorized.

## Implementation — 2026-09-29

Claimed at `d25f17b69`; candidate `9ad03a828`. macOS arm64, Qt 6.11.2, dev preset.

### Design

`review_ledger_application_host` is now an actor with a first-in, first-out
operation queue. The store is opened through a `review_ledger_store_opener`
and owned by a cell that only the effect lane touches, one operation at a time.
The profile adds one `ReviewLedgerEffects` worker lane, shared by its windows. It
is drained with the profile's other effect lanes and counted in their
quiescence. Each operation steps as follows:

1. Its first step runs on the decision lane when it reaches the head of the queue.
2. Storage work goes to the effect lane.
3. A typed continuation returns to the decision lane, which decides, retains
   and then delivers the terminal.
4. The next operation starts after that terminal.

The core decides a receipt command at the head of the queue, against the
durable result of every operation before it. A material carry is two steps of
one operation (load, then the core's carry, then commit). A conversation Mark
read on a cold cache loads and then writes within one operation. At most 64
operations are admitted; the one beyond is refused `unavailable`, and a receipt
command's refusal is presented. There is no mutex. Every member is
decision-lane-only, and reads answer from state already loaded or committed.

`retire()` answers queued operations `cancelled`. A completion arriving after
it is counted `LateCompletions`; nothing is retained, published or failed. The
store is released on the effect lane once no operation is running. The host's
destructor retires, and the implementation stays alive, through its closures,
until its last terminal. `MaterialGeneration` means only the latest material
observation (or `retain`) retains and publishes. A read that began before a
sibling invalidation retains nothing.

The composition's `ledger_answer` delivers an answer only while the window,
through its `WindowLifetime`, and the ledger generation that answered are both
current.

### AC1 — inventory of every ledger call

| Call (before) | Now |
|---|---|
| Store open in `compose_review_ledger` (sync `make_review_ledger_store`) | Opener on the effect lane; `review_ledger_opened` sets availability and republishes read state, private records (correlated) and the overview. The private review store opens after this answer, because it refuses the same file changing under its preflight |
| `dispatch_receipt` → `submit` | Queued; published once durable; its answer refreshes overview receipts |
| `observe_selected_material` → `observe_material` | Queued load, then carry and commit when the material changed; the answer re-presents the draft |
| `refresh_shared_ledger` → `refresh_receipt` | Queued; a failure refreshes overview receipts |
| `publish_overview` / `refresh_overview_receipts` → `load_receipt` | `cached_receipt` (memory) plus one `load_receipts` operation for every miss, then republish |
| `observe_read_state` / `publish_thread_read_state` → `observe_conversations` | `unread_conversations` (memory); a miss calls `request_conversation_receipts`, which loads once and republishes, including the queue's unseen count |
| `mark_conversation_read` | Queued (loads first on a cold cache); the thread and the queue are republished once durable |
| `private_observation` (defects, notes, overview, agent context, next note id, chat prose) | In-memory mirror loaded at opening and on a sibling `PrivateRecords` change; this host's own writes adopt into it on completion |
| Note persist/erase (`dispatch_private_note`) | Queued; acknowledged or refused on the answer |
| Defect persist (classify, `dispatch_defect`, correlation) | Queued; kept and presented once durable; a second gesture on a defect whose write is pending is ignored (`PendingDefectWrites`) |
| `admit` / `session_catalog` (`agent_session_catalog_port`) | Port is now asynchronous. `agent_invocation_host` starts a turn when the identity answers, refuses a second turn meanwhile (`AdmissionPending`), refuses a stale binding, and carries a cancel or shutdown to that turn |
| Agent finding "add to private notes" (direct `store_note` from the lane) | `agent_finding_host` takes a promoted-note store that writes through the ledger host; the outcome is presented on the answer. `promoted_note` builds the record, and `store_promoted_finding` remains for direct store tests |
| Last-reviewed head / defect erase | Async host operations (only fixture callers today) |
| Fixture seams (`mark_head_reviewed`, `seed_agent_private_context_canary`, `agent_invocation_context`, `agent_session_catalog`) | Bounded (10 s) waits for the answer on the decision lane, test builds only; the seeds also wait for the ledger to be quiescent |

No call site reads the store synchronously any more; `rg -n 'observe_conversations|->load_receipt\(|ReviewLedger\.|unavailable_review_ledger_store' src` finds nothing.

### Verification

| Criterion | Check | Result |
|---|---|---|
| AC1–AC3 host | `application_review_ledger_host` (`tests/application/review_ledger_host_tests.cpp`, 24 courses) | Pass. The earlier synchronous courses are ported to manual lanes: ordering `request, committed, published`, refusals, failures, private round trip, conversation load-once and Mark read, notes, sessions, sibling refresh, durable receipt cache. New courses: **held store** — a real worker thread blocks in a gated store while the decision lane runs posted work, answers every read, and admits a second gesture in under 500 ms; nothing is published until release, then the queued gesture is decided against the first commit. **Negative control** in the same course: with storage run inline, the gesture holds the lane for the full 1.5 s gate (1505 ms inline against 0 ms with the worker). Also: bounded admission (third of capacity 2 refused and presented); FIFO rapid gestures with one failed commit rolling back only itself; retirement cancels queued work and refuses the late completion even though the store committed it, then closes the store on the effect lane, and a successor on the same root recovers the commit; shutdown drains each terminal exactly once; an unopenable store fails every operation typed; an older material observation does not publish; a read overtaken by an invalidation retains nothing; a cold Mark read loads then writes |
| AC3 host negative control | `resume` without its retirement check | The course fails: "the late completion is refused: nothing retained, published or failed into a successor" |
| AC2 agent sessions | `application_agent_invocation_host` (new `test_session_admission_answers_later_and_admits_one_turn`), `application_private_review_host` | Pass. The catalog answers later; one turn is admitted; a binding that moved during admission starts nothing |
| AC3 composed | `application_active_changed_scaling` (515 files, 1,000 threads), new held-ledger step | Pass. The profile's real ledger lane is held with `hold_review_ledger_effects`, then Mark unread. A posted task runs in 1 ms, Overview and Read switch, the gesture is decided (`Commands` +1) with `Completions` unchanged and the shown receipt unchanged; after release it is durable, then published, written exactly once |
| AC3 composed negative control | Same course with the composition passing an inline effect executor | The course fails: "the held gesture is decided and queued, and nothing is written or published before the store answers" (`completions=1` while held) |
| AC2/AC3 composed receipt, conversation, cold-cache and sibling paths | `application_review_ledger_composition_contract` (seed, reopen, unavailable), `application_threaded_reply_contract` (github-com, ghes-3.18; includes BPR-1214's Discard/Back), `application_overview_since_review_contract`, `application_profile_multi_window_public_path` (receipt, conversation and defect fanout across sibling windows, a failed sibling reload under a held lane, PR replacement), `application_profile_two_account_isolation_public_path`, `application_setup_destination_public_path` | Pass |
| Regression sweep | Full `ctest --preset dev` at this candidate and at baseline `d25f17b69` (a separate worktree built from the same commit) | The same 43 tests fail in both, and none newly fails. `application_review_tour_writing` differed only because `test_scoped_writing_agent` does not build in this checkout's configured `build/dev` (see below); with the baseline-built fixture agent it passes. Composed failures common to both were rerun with output. After the adjustments below, every one fails at the same step with the same evidence as the baseline |
| Gates | clang-format on all touched C++ files; clang-tidy on the four application TUs, the composition and the three end-to-end tests | Format clean; no tidy finding on a changed line |

Course adjustments made because work is now asynchronous:

- The ledger probe gives each gesture a quiescent ledger and reads the private
  writes' answers.
- The defect probe reads the classification, verification and
  reclassification once they are durable. It fails where the baseline fails:
  `DEFECT_SHARED_READY_STALLED`, evidence `11111100011`.
- The multi-window course holds the ledger lane, so its read fault still lands
  between the writer's commit and the sibling's reload.
- The two-account and multi-window courses wait for the ledger to open, and for
  a classification to be durable.
- The agent course waits for the typed draft to be accepted before selecting
  notes. It fails where the baseline fails: the private-run refusal
  "Choose immutable revisions…".

### Limits and what this does not claim

- **Platforms.** macOS arm64 only. Windows and Linux runs are not done.
- **Timing.** The 1 ms and 1505 ms figures are observations on this machine.
  They are not latency budgets; BPR-0967 owns those.
- **Queued work at retirement.** Retirement cancels queued work, and a PR
  refresh retires and recomposes the ledger as it did before. So a gesture
  still queued at that instant is answered `cancelled` rather than written.
  Nothing is lost that was already written.
- **Pinned composition contract.** `application_review_round_trip_composition_contract`
  was already failing at `bd6b7a363`. Its github_application.cpp digest pins
  `aa8826e5a`, and `27dbeef51`, `637cd41df` and `eca094604` changed that file
  without a reseal; a literal alignment also drifted. This card changes the
  production composition, so it adds to that drift. The reseal is left to the
  owner of the unreviewed tour changes. This card's own change is reviewed
  here; it adds no credential, provider, transport or mutation path.
- **`test_scoped_writing_agent`.** In this checkout's `build/dev` it compiles
  as `-std=c++98`, because the target declares no C++ standard, and it fails.
  A fresh configure compiles it as C++14 and it passes. This predates the card
  and is outside its scope.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, Qt 6.11.2, offscreen,
`build/dev`, at `22d18416` and then the fix at `52fa4a06`.

| Row | At `22d18416` | At `52fa4a06` |
|---|---|---|
| `application_review_ledger_host` | Pass | Pass |
| `application_active_changed_scaling` | **Fails 5 of 5**: `unchanged history refresh reads no Overview files or receipts and rebuilds no file projection` (`overview_builds=1 overview_receipt_visits=515 receipt_loads=2`) | Pass, 5 of 5 |

**Cause.** The failure is in the course, not the product. The row passes on
Linux at the base `d25f17b6`. A temporary trace of the callers of
`publish_overview` showed where the extra build came from. The queue refresh
reopens the ledger, and its `load_private` answer and the `load_receipts`
answer that follows arrive on the decision lane after the course takes its
`before` sample. That was synchronous before this card. On this station
they landed inside the unchanged epoch refresh's window and were counted
against it. The rebuild is correct asynchronous work that belongs to the
earlier gesture.

**Fix (`52fa4a06`).** The course settles before sampling, and `settle` now
also watches `Ledger.Operations` and `Ledger.Completions`. No counter or
expectation was relaxed. The epoch refresh then reports `overview_builds=0
overview_receipt_visits=0`, the same as the base.

The composition-contract rows fail at `22d18416` exactly as they fail after
the fix: `application_workbench_composition_contract` and
`application_review_ledger_composition_contract` pass, and the other seven
fail at both. Timings are observations, not budgets. The traced build was
never committed. The logs are station records in
`build/logs/bpr-station-1211-1213-1214/` and are not committed.
