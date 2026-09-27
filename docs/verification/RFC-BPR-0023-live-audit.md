# RFC-BPR-0023 live audit and execution amendment

BPR-0576 / milestone `m1-live-audit` / mandate `r1`, 2026-09-26.
Source and tracker: `/Users/jaeger/.codex/worktrees/15a6/BetterPullReview`;
branch `codex/rfc-bpr-0023`. The coordinator owns main integration.
Current continuation uses owner amendment **r2** (2026-09-26): implement now,
with one capability-critical independent review of the final integrated candidate
at M10/BPR-0591, including the architecture boundaries formerly gated at M2.
The original r1 evidence remains historical; no review has been supplied or waived.

## Authorization and baselines

The owner now requests implementation of RFC-BPR-0023, revision of the plan for
the evolved application, and landing each accepted milestone on main. This
supersedes the 2026-09-04 implementation deferral. It is authorization, not
feature acceptance, native evidence, independent review or permission for live
GitHub/GHES mutations. No pushes or PR actions are authorized for this lane.

- Import baseline: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.
- Import commit: `7407f606087473c0b6246f550063dc72ee178a38`.
- Immutable implementation `RFC_BASE_SHA`: `899090f3` (full identity in the
  [verification ledger](RFC-BPR-0023.md)). The old import identity is retained.
- The [source-only issue plan](../rfcs/plans/RFC-BPR-0023-issue-plan.md) and
  [retained source audit](../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1/supporting/CURRENT_STATE_SOURCE_AUDIT.md)
  remain byte-identical. Their line numbers, schema numbers and defer wording
  describe the import baseline.
- The [adopted execution plan](../rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json)
  preserves all sixteen whole-card obligations and all eighteen RFC criteria.
  Its preparation receipt retains the original pack implementation-order text
  and digest (copied without changes into the helper's temporary source root);
  its source receipt identifies the engineer-authored normalization. This
  document and the verification ledger carry live audit/continuation updates.

There was no prior implementation handoff. The coordinator's resolved strategy
snapshot is retained beside the ledger. No RFC-0023 human candidate, protected
review session or background command was inherited. The only pre-existing
working file was that selection snapshot, authorized for inclusion by its owner.

## Dependencies and first boundaries

RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0011 and RFC-BPR-0012 are all
`Auto-Verified` at the implementation baseline. Their formerly open closeouts
BPR-0242, BPR-0245 and BPR-0507 are Done. BPR-0568, BPR-0569 and BPR-0571,
the explicit RFC-0022 joins required by BPR-0584/BPR-0587, are Done. RFC-0022
itself remains Proposed with its external evidence/closeout pending; this is
not a new blanket dependency on local thread work and grants no acceptance to
RFC-0023. The existing card edges remain unchanged.

The ten milestone outcomes are: live audit; architecture allocation; core
semantics; storage; protocol metadata; thread host; composition/publication;
Qt/public composition; failure/rendered acceptance; documentation/independent
review/closeout. Storage and protocol metadata both follow core semantics;
thread-host work joins them. Execution is serial by default even where the
plan expresses independent prerequisites. The original fourteen prose slices
are preserved as work within these outcomes, with the detailed sixteen cards
as acceptance units. The retained pack phases map as 0→M1/M2, 1→M3, 2→M4, 3→M5, 4→M6/M7,
5→M8 and 6→M9/M10. Splitting the original initial planning step into audit
and allocation gives this assignment its requested bounded first milestone.

M1 ends when the audit, complete evidence ownership, plan adoption and focused
documentation/bookkeeping checks pass. It adds no product feature. M2 owns
BPR-0577's detailed signatures, transaction choices and navigation allocation.
Under r2 its independent authority-boundary review is carried to the final
integrated-candidate review at M10/BPR-0591. That review remains pending and
cannot be self-certified. Subsequent owners claim only eligible
cards; cards remain Backlog until their own dependencies permit readiness.

`NEXT_ID` is BPR-1221 at this audit; existing imported IDs remain unique. No
new IDs, reservations, platform exceptions or schema numbers are allocated.

## Live source audit against every imported surface

Paths below are repository-relative; symbols identify the inspected source at
the implementation baseline, not historical line numbers.

| Imported surface | Current observation | Required disposition / owner |
|---|---|---|
| `src/application/include/bpr/application/review_ledger_port.hpp`: `local_note_record` | Still a flat note with exact optional file/comparison attachment; now also has `local_note_revision`, expected-revision edit/delete and typed conflict carrying the durable competitor. No actor/history/run linkage. | Preserve bytes, attachment, old identity and v6 conflict semantics; migrate to `legacy_unknown` without inventing deleted historical versions. BPR-0577/0580/0582. |
| Same header: `private_ledger_observation` | Still aggregates bounded Notes/Defects/LastReviewedHead; limits are 4,096 notes, 4,096 defects. | Keep it a compatibility observation, never an all-turn transcript load. New narrow cursor/body/provenance ports belong to BPR-0580. |
| `src/adapters/sqlite/src/review_ledger_database.hpp` | Current ledger version is **6**, not 5. | Next candidate is 7 only if the frontier is unchanged when BPR-0580 begins; this audit reserves none. |
| `review_ledger_database.cpp`: canonical schema and `establish_schema` | Origins 1–6 migrate forward; v5→v6 adds note revision and `local_note_sequence`; current note revision high water survives deletion. Agent session/sequence tables remain; no thread/run/composition authority. | Test each origin, canonical STRICT schema and wrong-app/future refusal; preserve receipts, defects and non-pruned allocation. Atomic migration retires `local_note` as authority. |
| `src/core/include/bpr/core/review_round_trip.hpp` | Draft still owns exact line/body/state, now with optional `ReplyToThread`. Root publication is right-side; replies may be left-side under existing authority. | Retain RFC-0008 target vocabulary. Required RFC-0023 root course remains exact current/right. A PR/range/historical subject grants no new mutation target. |
| `src/app/src/github_application.cpp` draft allocation | `NextReviewDraft` is gone. Profile runtime owns `shared_draft_application_host` per authority; that host holds volatile `NextDraft`, at most 64 drafts and 32 consumers. `revisioned_review_draft` supplies scope/revision CAS and a 64 KiB body ceiling. | Extend the existing shared host and reducer with durable IDs/correlation; preserve sibling-window fan-out, rejected edits, account/head partition and publication terminal handling. Do not add a second draft state machine. BPR-0577/0580/0583. |
| `release_update_recovery_store.cpp` | Separate updater recovery stores inert draft description/body/indeterminate state. It is not exact ordinary draft/anchor/composition restoration. | Keep its boundary; account for new private drafts in updater preservation without treating recovery text as draft publication authority. BPR-0583/0587/0590. |
| `agent_profile.hpp` | Profiles still carry executable/protocol/workspace/environment **names**; no configured/requested/reported runtime model or effort facts. | Add bounded typed optional facts only where supported. Names never prove actual model. BPR-0579/0581. |
| `agent_session.hpp` and ledger session admission | Session remains exact revision-pinned, with independent durable session sequence and retention of 128 sessions. | Each invocation needs a separate durable run; retained run snapshots outlive session pruning. BPR-0579/0580/0581/0582. |
| `agent_launch.hpp/.cpp`: decoder | App Server tracks remote thread/turn; JSONL tracks remote thread; Claude tracks remote session. The shared decoder has a RemoteSession member, but App Server does not populate it. These are decoder state, not exported provenance. | Surface supported bounded typed metadata with basis; do not turn framing IDs into authentication/model proof. BPR-0581. |
| Same files: `agent_run_result` | Result contains Session/Outcome/Events/stderr/exit/protocol-failure/findings, no distinct durable run or model/effort/version facts. Protocol output events exist. | Capture complete assistant bytes from validated output/terminal framing, not concatenated finding claims or raw log persistence. BPR-0581/0582. |
| `agent_chat_session_host.hpp/.cpp` | One existing invocation path remains. Chat now retains up to 16 in-memory `agent_chat_reply` rows assembled from validated finding claims; oldest reply is dropped. No prompt or complete-response transcript is durable. | Preserve current session policy while committing prompts/run admission before dispatch and retaining complete correlated responses. Temporary reply IDs are not durable turn/run IDs. BPR-0582. |
| `agent_finding.hpp` | Structured finding still retains provider, analyzed revisions, anchor, claim/evidence/consequence and suggested comment. | Snapshot exact validated data under run + ordinal + digest. Never infer identity from session + ordinal. BPR-0579/0582. |
| `agent_finding_host.hpp/.cpp` | Current delivery and explicit promotion remain; note promotion is revision-aware but still flattens source information. | Reuse explicit actions, retain source snapshot/lineage and connect workshop acceptance to the one draft authority. BPR-0582/0583. |
| `review_mutation_port.hpp` and `review_mutation_recovery_store.cpp` | ServerCommentId still exists; recovery journal is now **v5**, preserving ordinary and whole-review attempts plus RFC-0022 logical native/file representation recovery and durable public body. | Consume the logical canonical receipt, exact approved/wire digests and reconciliation; local lineage repair must never POST. BPR-0584. |
| `src/adapters/qt/src/side_chat_panel.cpp` | Recent reply list and promotion exist, but `present` clears/rebuilds this small temporary list when replies change. | Replace/bridge with stable-identity paged transcript updates; bounded temporary list behavior is not RFC-0023 incremental transcript evidence. BPR-0585/0586. |
| `src/adapters/qt/src/workbench_window.cpp` | Overview/Read/Review/Discuss modalities coexist with Comments/Notes/Tour tray tabs. Notes holds private notes/agent/chat/findings; the draft and workshop have separate center composition pages. New PRs open on Overview. | Allocate coherent private/shared navigation against this UI; enter the appropriate modality before evidence interactions. Preserve exact selected tab/comparison and avoid another stacked transcript panel. BPR-0577/0585–0587. |

## Shortest public paths inspected

The source trace is through the production `github_application.cpp` composition:
`side_chat_panel::send_action` → `dispatch_side_chat_intent` →
`agent_chat_session_host` → its turn callback → `dispatch_agent_invocation` →
`agent_invocation_host` → the existing agent process port → serialized projection
and finding/chat delivery. In the current callback, IncludeNotes expands to
`all_private_note_ids()`. New private-thread context must instead carry the exact
selected immutable revisions and reload/revalidate them at dispatch. A broad
checkbox must never silently select the new transcript history.

Finding promotion reaches the existing finding/workshop hosts and draft editor.
The editor's revisioned command reaches the profile shared draft host; separate
Publish reaches `review_mutation_application_host`, its coordinator/provider and
logical recovery/reconciliation. Whole-review submission, resolve and Tour
publication are additional existing mutation capabilities: the zero-mutation
canary must count all of them, not just inline POST.

This is source-level reachability evidence for planning. This checkout has no
configured `build/` tree; no product runtime was built or exercised for this
documentation-only milestone. Future behavior cannot inherit a pass from this
trace. Reuse the existing public composition courses (including construction
cuts) as starting points and add the actual new workflow course as it becomes
reachable, rather than postponing every public-path check to the final UI card.

## Architecture and verification allocation

| Owner | Planned narrow responsibility | Direct evidence owners |
|---|---|---|
| Core | Strong private/run/composition/lineage IDs; actor and provenance basis; immutable history/DAG; exact subject/stale policy; three composition modes and target partition; local-only accept transition. | BPR-0578/0579; replay/properties and forbidden dependencies. |
| Application | Private host performs bounded store effects, prompt-before-launch, run correlation, explicit source admission and serialized completion; shared draft host remains the draft owner; existing mutation host remains publication owner. Every request includes exact profile/account/review, request/session/run, revision and generation as applicable. | BPR-0582/0583/0584; manual executor, ordering, cancellation, identity retirement and fault injection. |
| SQLite | One private-content authority, durable high water, exact migration, transactional accepted composition/draft linkage, bounded cursor/body reads and local lineage repair. Recovery journal remains the remote-attempt authority. | BPR-0580/0588; all-origin schema, restart/process-kill, concurrent opens, quota and purge. |
| Agent | Extend the single request/decoder/result path; snapshot profile/settings facts, aggregate eligible complete output, retain unknown/conflict cases. No new launcher, model API or secret-bearing logs. | BPR-0581/0582/0588; all four protocols, malformed/oversize/stale/cancelled terminals and child-input canaries. |
| Qt | Stable-identity catalog/transcript/history/detail/source selection and local composition projections; exact view context and separate accept/publish actions. | BPR-0585/0586/0589; public interaction, accessibility, scales, themes, reset counters. |
| Composition | Profile storage ownership, per-window consumers, agent/store/draft/mutation wiring, public reachability and complete shutdown drain. | BPR-0587/0588; production composition, restart, all mutation counters and negative construction controls. |

The eighteen-row claim matrix in [RFC-BPR-0023.md](RFC-BPR-0023.md) remains the
canonical acceptance ledger. No AC row passes in M1. BPR-0577 must refine this
allocation into concrete contract signatures and transaction boundaries before
implementation. Owner amendment r2 combines its authority-boundary review with
BPR-0591's one required final capability-critical independent review.

## Schema and migration evidence matrix

| Starting authority | Planned coverage |
|---|---|
| Empty ledger / v1 / v2 | New canonical schema; receipt identities and absent-note semantics preserved. |
| Ledger v3 / v4 / exact v5 | Byte/attachment/ID-preserving `legacy_unknown` conversion; pre-existing defect migration and session semantics retained. |
| Ledger v6 | Preserve positive current note revision and deletion high water; stale sibling edit/delete must fail after conversion; no invented history and no ID reuse. |
| New schema | Concurrent stores, whole-operation quota failures, cursor progress, cross-review refusal, foreign keys, canonical STRICT inventory, crash/WAL recovery and one private authority. |
| Future or wrong application | Refuse before mutation; compare bytes/schema after refusal. |
| Mutation recovery v5 | Reuse ordinary/whole-review/logical recovery. If a change is needed, recheck/allocate that store's own next version and preserve all supported origins; never reuse ledger version as journal version. |
| Updater draft recovery | Inert salvage remains separate; verify it does not reauthorize a publish or substitute for exact durable draft restoration. |

IDs use dedicated non-pruned high-water rows per exact review scope, advanced in
the same transaction as admission. Do not scan retained maxima during ordinary
allocation. The v6 note revision sequence is a concurrency fact to preserve,
not a pre-existing thread/run allocator. Deletion/purge cascade policy must name
references from retained compositions, drafts, findings and publication lineage;
M2 settles that policy and M4/M9 exercise it.

Keep RFC bounds: 4,096 threads; 65,536 revisions; 16 KiB reviewer/composition
body; 1 MiB complete assistant body; 64 MiB retained bodies per review; page at
most 64 revisions/512 KiB; at most 256 composition sources. Body continuations
carry immutable identity, total length, digest and advancing cursor, so two
bounded reads can retrieve a 1 MiB body without declaring a partial body complete.
The existing supplement admits only 64 notes/64 drafts with 16 KiB item bounds
and a separate 256 KiB selected code-text bound; the decoder has a 64 KiB frame and 1 MiB total stdout ceiling. These are output
bounds, not a total encoded-input contract. The inspected launch request has an
Input string but no named total-input admission bound; BPR-0577 must allocate
that bound explicitly, and
BPR-0581/0583 must refuse before dispatch when an otherwise storable source set
cannot fit. Storage capacity is not invocation capacity. The existing shared
draft's 64 KiB limit does not widen this RFC's 16 KiB composition limit.

Benchmarks report fixture size, DB growth, peak memory, page/body bytes,
visited rows, visible/changed records and model reset counts; timings remain
machine-specific. Ordinary turn updates must scale with active plus changed
sets and cannot reload all private records or shared conversations.

## Protocol capability plan

These are observations of the current decoder, not claims about every installed
external tool version. BPR-0581 must bind changes to exact protocol fixtures.

| Protocol | Facts already tracked | Model/effort/version today | Required extension/evidence |
|---|---|---|---|
| Codex App Server | Decoder-owned remote thread/turn and terminal status; no observed remote session fact. | No exported typed provenance. | Export supported IDs/facts from admitted envelopes; prove complete/partial/conflicting/absent and malformed framing. |
| Codex JSONL | `thread.started.thread_id`, phased turn/item events and complete agent-message output. | No exported typed provenance. | Preserve thread fact, distinguish output completion and tool-supplied settings; never fabricate a remote turn ID. |
| Claude stream JSON | Exact `session_id` across init/assistant/result and bounded assistant text events. | No exported typed provenance. | Capture only actual supported envelope fields; missing effort/version remains absent. |
| External terminal | Bounded text stream; no structured finding-result carrier. | Unknown. | A distinct local run and terminal outcome are still available; unsupported remote metadata remains unknown. |

Requested settings must record what was actually admitted and sent; configured
facts describe genuine explicit defaults, never display/executable labels.
Protocol-reported values are separate; content-self-report remains absent unless
an explicit low-trust parser is deliberately introduced. Do not persist stderr,
raw frames, environment values or credentials for apparent provenance completeness.

## Designated publication and review plan

BPR-0584/BPR-0588/0589 own synthetic native and file-permalink courses, exact
public body/actor/server-ID inspection and remote-success/local-failure recovery.
No safe live target or signed independently readable RFC-0023 recording was
supplied to this assignment. Build synthetic fixtures now when their feature
exists; at the external criterion use an owner-designated safe target or the
RFC's permitted signed recorded fixture plus independent reader. An unsigned
local mock or prior RFC-0022 acceptance cannot satisfy RFC-0023 lineage evidence.
A local deletion/database-loss course must inspect unchanged shared state and
prove zero mutation; remote state comparisons require the designated authority.

Required rendered/native/human evidence is prepared only against a real feature
candidate. Use the configured attended-review companion when a human criterion
is ready, one Accept/Reject question per criterion, with absolute evidence paths,
exact candidate identities and a loopback HTML handoff. M1 has no human decision
ready and therefore starts no review session. Under amendment r2, BPR-0591
owns the one pending final independent review including BPR-0577's architecture
boundaries; this leaf does not claim independence for its own audit.

## Documentation impact ledger

| Concern / owning document | M1 disposition | Implementation owner |
|---|---|---|
| Private authority, schema, migrations, IDs, backup/purge: `docs/architecture/review-ledger.md` | Planning only; current shipped notes/CAS facts unchanged. | BPR-0580/0590. |
| Drafts, body bytes, acting identity, native/file publication/recovery: `docs/architecture/review-round-trip.md` | Planning only; preserve existing mutation authority and logical journal. | BPR-0583/0584/0590. |
| Agent profile/protocol/context/privacy: `docs/architecture/ai-agent-boundary.md` | Planning only; future provenance cannot overstate current decoder. | BPR-0581/0582/0590. |
| State and component ownership: `docs/architecture/domain-and-state-model.md`, `docs/architecture/system-architecture.md` | No shipped behavior changed; allocation above is proposed until implemented. | BPR-0577 onward / BPR-0590. |
| Product/workbench/operator, private/shared labels and UI commands: `docs/architecture/product-contract.md`, `docs/ux/`, `README.md` | No current feature claim; account for current modalities, tabs and profile windows. | BPR-0585–0587/0589/0590. |
| Storage, diagnostics, export/deletion: `docs/development/storage-and-diagnostics.md` | No new retained data yet; future ledger content remains private and diagnostic outputs use IDs/digests. | BPR-0580/0588/0590. |
| Test guidance and performance: `docs/development/build-and-test.md`, `docs/verification/fixture-catalog.md` | New evidence plan only; exact registered selectors below. | BPR-0588/0589/0590. |
| GitHub/GHES authority, credentials, network isolation | No M1 effect or contract change. Existing all-mutation and secret canaries must cover new joins. | BPR-0584/0588/0590. |
| Git cache, packaging and launch | No M1 change to caches/binaries/helpers/dependencies. Revisit if implementation changes composition or installed paths. | BPR-0587/0590. |
| Accessibility, themes, native platforms | No M1 runtime acceptance. Preserve Qt/native style and target-native limitations. | BPR-0589/0590. |

## Focused verification selection

M1 runs the configured documentation gate, documentation regression tests,
scoped issue-board validation, adopted-plan/binding checks, exact retained-pack
hash checks and `git diff --check`. None certifies feature behavior.

Future owners select registered courses by composition and inspect the actual
CTest inventory after configure. Existing starting selectors are
`core_review_round_trip`, `core_agent_chat_scope`, `core_agent_finding`,
`core_agent_finding_promotion`, `sqlite_review_ledger_storage`,
`application_agent_finding_protocol`, `application_agent_chat_replies`,
`application_agent_chat_composition_contract`, its stale/continued/anchored and
construction-cut courses, `application_agent_finding_composition_contract`,
`application_review_round_trip_composition_contract`, and
`qt_composed_github_scale_100/125/200`. These are reuse candidates, not executed
M1 evidence or substitutes for new private-thread/migration/public-path assertions.


## M2 — live architecture contract

BPR-0577 / `m2-architecture` / mandate `r2` (review timing amendment), implementer
`/root/rfc0023_m2`. Re-audited against M1 integration
`cb36559911d6cd4def1582a0121b527ed2f9edce`; source is unchanged from the
implementation baseline. The contracts below are the implementation allocation,
**not shipped feature claims**. Independent authority-boundary review is still
required at M10/BPR-0591 on the final integrated candidate under owner amendment
r2. M2 acceptance covers architecture allocation and its checks. Exact candidate,
amendment and disposition records live in the verification ledger.
No imported plan, criterion, dependency or M1 acceptance is superseded.

Two M1 inventory details are corrected above from source: the fourth modality is
Review, while Tour is a tray tab; App Server supplies thread/turn IDs, while the
shared decoder's session member is populated by Claude. Also,
`maximum_selected_source_bytes` currently bounds selected **code text**, not the
sum of selected private notes/drafts. M2 adds a separate total-input contract.

### Ownership and concrete extension seams

Existing names below are source symbols; names marked **new** are the allocated
interfaces to implement, with C++ spelling adjustable within the owning card.
There is no new executable, universal service bus or model API client.

| Owner and existing seam | Allocation | Card |
|---|---|---|
| `src/core/include/bpr/core/review_ledger.hpp`, `agent_session.hpp`, `diff_semantics.hpp` | **New** `private_review.hpp/.cpp`: strong IDs, exact/legacy binding, typed subject/actor/revision, lifecycle and compact interaction reducer. Domain values own no store or process. | BPR-0578 |
| `agent_finding.hpp`, `revisioned_review_draft.hpp`, `review_round_trip.hpp` in the same core include directory | **New** `review_composition.hpp/.cpp`: provenance facts, source descriptors, DAG order, mode/target partition and local draft acceptance proposal. Extend the existing revisioned draft value with optional exact composition linkage; retain `core::apply` as draft policy. | BPR-0579 |
| `src/application/include/bpr/application/review_ledger_port.hpp`; `src/adapters/sqlite/src/review_ledger_database.cpp` | **New** narrow `private_review_store_port.hpp` and draft persistence facet on the same ledger transaction owner; SQLite implements them. Existing receipt/defect/session ports keep their authorities. | BPR-0580 |
| `src/application/include/bpr/application/agent_launch.hpp`; `src/application/src/agent_launch.cpp`; `src/adapters/agent/` | Extend request/decoder/event/result with exact local Run and qualified metadata. The existing `agent_process_port` remains the only process capability. | BPR-0581 |
| `agent_chat_session_host`, `agent_invocation_host`, `agent_finding_host` in `src/application/` | **New** `private_review_application_host` orchestrates durable thread operations, source admission and result ingestion. Chat retains session/stale-continuation policy; invocation owns workspace/process lifecycle; finding validation stays in its existing core/host. | BPR-0582 |
| `shared_draft_application_host`, `agent_comment_workshop_host` | **New** `review_composition_application_host` orchestrates source/candidate work and sends local acceptance to the existing shared draft owner. It receives no mutation capability. Workshop and finding promotion use the same acceptance seam. | BPR-0583 |
| `review_mutation_application_host`, `review_mutation_coordinator::publish_attempt`, `review_mutation_submission`, `review_mutation_recovery_port` | Extend typed publication completion with the logical correlation facts below. A read-only receipt observer plus ledger writer repairs local lineage; it cannot submit mutations. | BPR-0584 |
| `workbench_window`, `github_conversation_model`, `private_notes_panel`, `side_chat_panel`, `review_draft_editor` in `src/adapters/qt/` | Private summary/transcript/history and composition projections replace the stacked private contents. Shared GitHub model stays distinct. Qt includes projection/intent contracts, never store, invocation or mutation ports. | BPR-0585/0586 |
| `src/app/src/github_application.cpp` profile runtime and window implementation | Profile owns one ledger service and shared draft owner per admitted scope; each window owns private/composition consumers and its existing invocation/mutation hosts. Wire typed callbacks and explicit executor/lifetime ownership here. | BPR-0587 |

**Current shortest public composition, inspected early:**
`side_chat_panel::send_action` → `dispatch_side_chat_intent` →
`agent_chat_session_host` turn callback → `dispatch_agent_invocation` →
`agent_invocation_host` → `agent_process_port::run`. The new durable admission
slots before that existing dispatch. `all_private_note_ids()` must leave this
path: a boolean cannot select a thread or its history. Publication remains
`review_draft_editor` → revisioned command/shared draft owner →
`review_mutation_application_host` → coordinator/provider → receipt and refresh.
M2 exercises these paths by source inspection only; no runtime feature pass is
claimed. M6/M7 must exercise their first reachable host compositions and M8 the
real window, using the existing construction-cut courses as starting points.

### Authority, identity and compact core state

The existing ledger `review_scope` is `(host, account, repository, PR)` inside
one BPR profile's private root. Keep that scope and its existing adapter
validation. Add a ledger incarnation supplied by the composition's entropy
capability when creating the new authority; all externally carried private
references include it. Deleting/recreating a database cannot make old references
resolve to newly allocated ordinal IDs. New IDs are independent strong unsigned
namespaces limited to positive `INT64_MAX`, allocated from dedicated non-pruned
per-scope high-water rows in the admission transaction. Zero, overflow and
exhaustion refuse; normal admission never scans retained maxima.

A new thread's immutable binding includes the profile/review scope, access mode,
authority fingerprint, exact base/head OIDs, retained epoch identities and exact
comparison identity where applicable. Live authorization additionally uses the
current application session, authenticated identity generation and window
context; none is restored as a permission after restart. A legacy note has an
explicit `legacy_retained_subject` binding containing only its known review and
optional file/comparison. Unknown OIDs/epochs stay absent, not filled from the
current PR. This preserves migration truth while all newly created threads
require exact revision observations.

The typed subject variant is PR-wide, exact file comparison, source line,
source boundary, selected range, group, segment, move relation, shared
conversation identity, or durable finding reference. Reuse existing domain
identities and bind range endpoints to one exact comparison. A display path or
row cannot serve as a subject. Availability (`current`, `stale`, `unplaceable`)
is observed/derived independently of `open`, `resolved_locally`, `archived`.
A historical subject remains inspectable and never changes to the new head.

One window's **new** `private_review_state` retains scope/binding, selected
thread/turn/revision, bounded cursor descriptors, one request identity and
generation per lane (catalog, transcript/body, mutation, agent admission), and
projected command/reason facts. `review_composition_state` retains one workspace,
mode, selected-set digest/count, optional exact target, candidate revision,
expected draft revision and pending operation. Full bodies, catalog rows,
protocol frames, source arrays and graph history remain host-owned bounded
observations or durable rows; they do not become reducer state. No global state,
clock, random generator, Qt, filesystem or provider is introduced into core.

All revisions have one typed actor: reviewer, exact durable run, or
`legacy_unknown`. An immutable vertex carries a per-review monotonic creation
ordinal shared across run-admission/revision/finding/composition vertices. A new edge may
name only an existing same-scope vertex with a smaller ordinal. Edges are
committed with their destination; later edge attachment is refused. Thus DAG
admission needs at most the selected 256 facts and does not load/transitively
scan all history. Editing appends a reviewer successor with `edited_from` and
an expected latest-revision CAS. A run may authorize an agent actor only for its
own complete/incomplete response or candidate under the recorded entry point.
No edit can rewrite predecessor bytes, actor, run or digest.

### Typed transitions, requests, completions and deltas

Every request context carries ledger incarnation/scope, application session,
window context, identity generation, request ID and lane generation. Add the
exact thread/header revision, run/session, composition/candidate, source-set
fingerprint and expected draft revision for operations that use them. Counters
are checked, never wrapped. A completion echoes the full context; current-lane
acceptance uses identity equality before looking at payloads.

| Intent / bounded observation | Core result / host request | Accepted terminal and projection |
|---|---|---|
| Create thread with admitted subject and reviewer body | `create_private_thread_request` with exact binding, actor and body descriptor | Store commits IDs/header/turn/revision/body atomically; `private_thread_committed` inserts summary and first revision. |
| Add/reply/edit turn; expected header/latest revision and selected source descriptors | `append_private_revision_request`; deterministic actor, predecessor, completion and DAG checks | `private_revision_committed` changes only that turn, its history count and summary. CAS refusal carries bounded current descriptor and preserves rejected editor text separately. |
| Resolve locally/archive/delete with expected version | `set_private_thread_lifecycle_request` or exact prepared deletion request | Local lifecycle/deletion delta only. No shared-resolution request exists in this variant. |
| Select thread/history/provenance or request continuation | `load_private_catalog/header/revisions/body/provenance_request` | Request-bound immutable page/body/detail observation; cursor/selection and changed identities only. |
| Send reviewer prompt or explicitly squash with agent | `admit_private_run_request` after mode/source/binding checks | Atomic prompt/run admission → `private_run_admitted` → existing invocation request; terminal maps to run/turn/candidate commit, never Publish. |
| Select sources and reviewer/copy/agent mode | `create_review_composition_request`; validate exact descriptors and target partitions | Composition/source rows and candidate revision commit; `composition_changed` with source/currentness/actor facts. |
| Edit or accept candidate with source/tip/draft expectations | `append_composition_revision_request` or `accept_composition_as_draft_request` | Shared draft owner stages core draft decision; one ledger transaction commits candidate acceptance and exact draft revision/link, then emits draft/composition deltas. |
| Separate Publish from existing draft editor | Existing draft `begin_publish`, followed by correlation precommit and existing mutation proposal | Remote receipt + successful refresh feeds local lineage commit; unavailable local persistence produces repair-pending, never another automatic POST. |

Diagnostics are typed and bounded: invalid identity/subject, wrong scope,
missing source, changed source, digest mismatch, incomplete source, stale target,
ambiguous target, cycle/order refusal, conflict, quota/input too large, unsupported
setting, metadata conflict, cancellation, unavailable and integrity failure.
Qt owns words and focus; deltas name semantic IDs, affected domains and command
reasons, never widgets or model indexes. No generic string command/map bus.

Effects run on the profile-owned serial ledger executor; responses post to the
window's serialized decision lane. Host staging is bounded to one active write
and one pending operation per window lane; excess ingress returns busy, not an
unbounded queue. Catalog invalidations coalesce by scope/changed identity using
`observing_review_ledger_store`'s existing fan-out seam. A sibling reloads only
its resident affected page/detail, not `load_private()` or every transcript.
Current receipt/defect compatibility behavior need not be broadly refactored
under this RFC; BPR-1211's older decision-lane ledger debt stays separate.

Cancellation before a ledger transaction commits changes nothing. A committed
write has one durable outcome even if its UI terminal is stale. Late terminals
cannot update another window/account; they may finish an already admitted run
against its original persisted binding, with stale/cancelled status visible on
later inspection. Duplicate matching terminals are idempotent by operation/run
identity; mismatched digests or conflicting terminals refuse. Shutdown closes
ingress, revokes source dispatch claims, cancels/drains invocation and mutation
jobs, commits admitted durable terminals, drains ledger effects and decision
callbacks, then releases projections, stores and profile root. Cancellation
cannot erase a possibly transmitted publication's recovery obligation.

### Ledger migration, draft ownership and retention

Rechecked frontier: ledger **6**, mutation recovery **5**. M4 rechecks and
allocates ledger's next version (7 if unchanged); M7 changes the recovery
version only if its separate schema actually needs extension. No number is
reserved here. Keep canonical STRICT schema validation, prepared statements,
foreign keys, application ID/future-version refusal and one atomic forward
migration. Preserve all supported v1–v6 origins and their receipt, defect and
session migrations.

Allocated ledger records are thread/header, turn/latest pointer, immutable
turn revision/body, derivation edge, immutable run admission plus terminal facts,
finding snapshot, composition/source set, immutable composition revision,
revisioned draft with optional composition reference, pending publication join,
publication lineage, legacy-note mapping and non-pruned high water/counters.
All keys/FKs include scope; mutable header/draft versions are CAS values.
Bodies exist once per authored revision; source lists retain IDs/digests and
bounded metadata, not copied source text. A linked durable draft references its
composition body; an ordinary draft owns its body. Candidate prose is an authored
new revision. SHA-256 body digests bind exact bytes with no whitespace or Unicode
normalization. Structured finding/selection digests use versioned, domain-tagged,
length-prefixed field encoding in canonical ID order; the separately retained
source presentation order cannot change identity silently.

Migration visits each legacy note once inside the migration transaction:
allocate a new thread, turn and initial revision; preserve its old ID in a
mapping; copy its optional file/comparison and text exactly; assign
`legacy_unknown`; and retain its exact positive v6 note revision as the facade's
expected-revision token. Preserve `local_note_sequence.last_revision`, including
a high water greater than every surviving row. A note edit through a temporary
`store_note` facade appends a reviewer revision and advances that same durable
CAS token; delete checks it. Old revisions absent from v5/v6 are not invented.
Use new identity namespaces, advance new high waters for every migrated record,
and drop `local_note` only after successful conversion/validation in the same
transaction. The mapping has no independent text column. The temporary note facade projects
only its mapped note threads, never all new transcripts; new facade creates also
write one canonical thread and a mapping. Post-migration schema
inventory must show exactly one private-content authority.

`shared_draft_application_host` remains the sole draft decision/fan-out owner.
Extend it with an asynchronous persistence facet: stage `core::apply`, commit
its expected-revision result, then publish to the existing 32-consumer fan-out.
Keep at most 64 active drafts per authority and the existing 64 KiB ordinary
draft ceiling; compositions retain their stricter 16 KiB ceiling. Durable draft
identity/revision and exact source target survive restart. Runtime
`review_draft_scope_id`, session/window/identity generations and publish grants
are freshly bound on attach. Retained drafts on an old head remain inspectable
but cannot become a current target by restoration. The updater's
`release_update_recovery_store` remains inert salvage; its text cannot restore
publication authority or become a second canonical draft store.

Acceptance uses one transaction in this same database for source existence and
expected-tip/digest revalidation, candidate acceptance, draft allocation/CAS,
and composition reference. Shared host state changes only after success.
Editing an accepted composition-derived draft must append a reviewer composition
successor and update both links in one transaction, so its composition revision
always identifies the current prose. A host receipt terminal likewise persists
before fan-out. There is no independent composition draft reducer or second
writeable draft cache. A failed store leaves the prior accepted version and
rejected editor text visible.

Retention is explicit, never automatic text pruning. Archive changes visibility
only. Deleting a selected thread cascades its own turns/revisions/edges, but
**refuses with a references-present result** while another retained thread,
composition, draft, finding or publication-lineage record needs its bytes or
identity. Offer exact dependent counts/IDs through bounded pages; deleting
those other objects requires separately selected local cleanup. No silent
cascade erases another thread or accepted draft. Unreferenced run/finding
records may be removed by that explicit cleanup; referenced provenance outlives
session pruning and has no cascading FK to the 128-row session catalog.
An active source dispatch claim makes deletion temporarily busy. Review-scope
purge removes all its private objects after producer quiescence, retains
sequence/incarnation tombstones within an existing ledger, and leaves the
mutation recovery journal's unresolved attempt authority intact. Whole ledger
loss creates a new incarnation. Neither operation calls any GitHub capability
or reconstructs private prose from the server. After ledger loss, an old journal
receipt remains unmatched local history; it cannot attach to a new incarnation
or fabricate the missing source/composition. Separate profile maintenance
continues to own its documented broader purge scope.

### Bounded queries, immutable body continuations and input admission

`private_review_store_port` exposes typed operations, not SQL or an all-record
aggregate: `list_threads(filter,cursor)`, `load_thread(id)`,
`list_revisions(thread,cursor)`, `read_revision_body(id,digest,offset)`,
`load_run_details(ids)`, `load_composition(id)`, `load_sources(exact_refs)` and
`find_publication(local_or_server_identity,cursor)`. Each bound is enforced by
the adapter as well as the host. No caller-selected unbounded limit. Write
operations return accepted identity/version plus changed counters, not history.

Keep the RFC's 4,096 threads, 65,536 turn revisions, 16 KiB reviewer/candidate,
1 MiB assistant, 64 MiB retained thread-body, 64-revision/512 KiB page and
256-source limits exactly. Add finite non-body bounds: 64 summary rows/64 KiB
metadata per catalog page; 16 run details/256 KiB per detail page; 65,536 runs,
65,536 finding snapshots, 4,096 compositions, 65,536 composition revisions and
65,536 publication joins/lineage records per scope; at most 256 edges on any
new revision. Other retained private payloads (candidate/draft/finding/provenance)
have a separate 64 MiB aggregate budget. Checked transaction counters enforce
count/byte admission including replacement deltas; refused writes commit no
partial payload, lost source or misleading success. These additional bounds do
not lower the RFC's thread-body budget. M4/M9 test each boundary and measure
actual metadata/index/database growth as well as body bytes.

Catalog cursors bind scope/incarnation, filter/order and catalog generation;
turn cursors bind thread/header revision and last monotonic order. Use indexed
keyset queries with limit+1, never growing OFFSET scans. A stale page cursor
refuses and offers an explicit fresh page while retaining visible content.
An isolated change may patch a resident row without refetching all pages.
A row too large for the remaining page body budget returns its exact descriptor
(length, digest, completion) with an explicit body continuation; it counts as a
row and advances the row cursor. This is visible unloaded content, not a partial
body advertised as complete.

A body continuation is `(scope, incarnation, immutable revision, digest,
total_bytes, next_byte_offset)`. Each successful nonterminal read returns
`0 < bytes <= 524288` and strictly increases offset; final offset equals total.
A 1,048,576-byte body therefore progresses `0 → 524288 → 1048576` in two reads.
Editing creates another immutable identity and cannot change these chunks;
deletion/mismatch refuses, with no empty-success loop. UTF-8 chunk boundaries
are assembled with an incremental decoder; digests and persistence remain over
original bytes. Transcript residency is one bounded page plus the explicitly
opened body/detail (maximum 1 MiB); scrolling does not accumulate all pages.
Copy/composition uses the full immutable body from the store and never the
loaded visual fragment. Exact-copy into a 16 KiB candidate refuses a larger
source whole; the reviewer can write a shorter candidate with derivation.

Add `maximum_agent_input_bytes = 8 MiB` at `agent_launch.hpp` and its builder/
process adapter boundary. It bounds all emitted stdin for one invocation:
encoded supplement, protocol wrappers, handshake/follow-ups and selected
private sources. The new selected-private raw-byte budget is **2 MiB** across
at most 256 exact references, independently of the existing 256 KiB selected
code-text limit and current note/draft per-item bounds. Store eligibility is
not agent-input eligibility. Count actual escaped wire bytes with a bounded
encoder before allocating/launching; an escaping-heavy set may fit 2 MiB raw
but exceed 8 MiB wire and must refuse. For App Server, reserve the maximum
encoded remote-thread-ID and fixed follow-up overhead before process creation,
then enforce the same remaining-byte counter on each actual follow-up. A late
invalid/oversize remote ID is protocol failure before any private context write.
Never truncate, silently omit sources, or count DeferredContext twice as if it
were itself another emitted frame. Raw private bytes are not passed in argv.

### Run metadata, response bodies and selected context

One invocation gets one durable `agent_run_id` even when reusing
`agent_session_id`. The existing invocation session catalog still owns session
admission; extend its orchestration with a run-admission port, not a second
launcher. Before dispatch, commit the prompt revision, exact run/session/profile
snapshot, entry point, binding, selected references/digest and admitted setting
facts atomically. The same ordering applies to a workshop/squash request even
when its source is a candidate rather than a chat prompt. A failed admission
launches nothing. Interrupted admitted runs recover as interrupted/unknown
execution, never automatically rerun and never fabricated complete responses.

Extend `agent_launch_request`, `agent_stream_event` and `agent_run_result` with
Run as well as Session. A **new** bounded `agent_runtime_metadata` value has
separate optionals for `requested`, `configured`, `protocol_reported` and
`content_self_reported` model/effort facts; each reported fact names its protocol
field/event basis. Requested means the setting actually admitted to the request;
configured means an explicit profile default, not an executable argument guessed
from a label. Unknown stays absent. Unsupported explicit requested settings
refuse before launch rather than pretending they took effect. Content-self-report
remains absent in the initial implementation; any later parser needs its own
low-trust contract and cannot populate protocol facts.

Metadata strings are bounded UTF-8 scalars: 256 bytes for model/effort/version/
profile display values, 1,024 bytes for each remote session/thread/turn/run ID;
at most 16 distinct reported fact records/16 KiB total metadata per run. Identical
repetition coalesces; differing reports remain separate with a typed conflict,
never last-write-wins. Wrong type, control/NUL content, wrong phase/identity,
excess count/bytes or inconsistent terminal data refuses metadata/protocol
admission explicitly, rather than dropping a fact and claiming completeness.
Optional absence alone never fails an otherwise valid run. Local IDs and
reported remote IDs remain distinct namespaces and confer no authentication.

The initial capability matrix is grounded in current decoder branches in
`src/application/src/agent_launch.cpp`; it promises no field an installed tool
has not actually supplied:

| Protocol | Exact current envelope/source to export | Honest initial absence / extension obligation |
|---|---|---|
| Codex App Server | Successful response `id:1/result.thread.id` → remote thread; `id:2/result.turn.id` → remote turn; `turn/completed` must echo thread/turn and completed status. | No remote session, run ID, model, effort or tool version is currently parsed. Do not relabel the local session or JSON-RPC request ID. M5 adds optional settings/metadata only against a recorded field mapping and fixture. |
| Codex JSONL | `thread.started.thread_id` → remote thread; `item.completed` agent-message item ID binds body pieces; `turn.completed` is terminal. | No remote turn/session/run ID or model/effort/version established by current decoder. Usage counters and item IDs do not supply those facts. |
| Claude stream JSON | `system`/`init.session_id` → remote session, checked again on assistant/result; assistant message text blocks are body events. | Current code does not parse model/effort/version. M5 may admit actual `message.model` or init metadata only with an explicit tested envelope basis; result prose is not such metadata. |
| External terminal | Local run/session, bounded text and process exit/cancel/timeout only. | All remote/model/effort/version facts stay absent; no structured findings carrier, label scraping or extra discovery process. |

BPR-0581 owns the exact extended field/setting allowlist and serialized fixtures
for each protocol before exposing it as a capability. Every optional fact keeps
its category even when conflicting. Full/partial/conflicting/unknown fixtures
prove the parser and labels, never external-tool-wide reporting. M2 settles the
field ownership and refusal/absence contract without claiming provider support
from synthetic input. Credential/environment values, raw frames and stderr are
excluded from durable provenance; diagnostics retain only IDs, digests and
closed reasons. Profile snapshots retain no argv/environment value dump.

Complete assistant text must come from validated assistant output, not finding
claims. App Server accumulates ordered deltas by admitted item identity;
JSONL retains complete `agent_message` items once per item ID; Claude retains
assistant text blocks once and does not append the `result` summary a second
time. Preserve the ordered bodies of multiple items and their boundaries in
bounded run output metadata; a deterministic concatenation for the turn adds
no fabricated prose. Duplicate/conflicting item deliveries cannot duplicate
text. A success terminal plus valid framing, bounded full output and matching
run establishes complete; cancellation/protocol failure/timeout is incomplete
or failed even if text exists. Existing stdout/frame limits stay 1 MiB/64 KiB;
the 1 MiB storage limit does not imply every protocol can frame that much prose.
Completeness and current-head staleness are distinct facts: a validated complete
response for an exactly correlated original run may remain complete and visibly
stale after navigation/head movement; it never becomes a response under the new
binding. Preserve the validated original output before the existing
`finalize_agent_run` stale presentation path clears stream events. A mismatched
run or incomplete/cancelled execution cannot gain completeness this way.
Partial output requires explicit reviewer adoption with an
`adopted_from_incomplete_run` edge before use as an eligible source.

A selected source descriptor is a tagged immutable revision/finding/composition
reference with scope/incarnation, ID, digest, byte count and the latest-tip
revision observed when selected. Historical selection is explicit; selecting
history does not accidentally select today's tip. Any successor/delete/digest
change since the selection was presented requires reload/reselection at dispatch
or acceptance. Sources known stale by head remain usable locally with the stale
badge, and can enter an explicit old-revision agent continuation only through
the existing chat policy; they never borrow a new exact binding.

On the serial ledger effect lane, reload only the exact selected descriptors,
validate count/bytes/completeness/binding and take a short source-use claim.
The run admission and input digest commit from that same checked snapshot.
Serialize competing edits/deletes with the dispatch claim through the existing
invocation's initial/deferred private input acknowledgement; they return busy
until input delivery or refusal releases it. Thus no gap lets an already
accepted delete/edit race the last dispatch check. The existing process timeout
bounds this claim even if App Server never requests context. Cancellation or
authority retirement revokes unwritten context before releasing the claim.
After input delivery, later edits create successors but cannot change bytes
already authorized for that run. Body buffers are released after input delivery;
no durable duplicate supplement is retained. Fixture canaries inspect the actual
child input for unchecked, removed, changed, duplicate and cross-review sources.

### Composition, exact targets and publication join

Source selection loads metadata for at most 256 immutable references. Core
partitions only exact eligible `source_line_ref` targets. PR/range/boundary,
shared-thread and historical subjects grant no root publication target.
Multiple targets require explicit choice or Split into Drafts; unplaceable
members remain visibly unassigned, never silently dropped. Split preview names
every partition and unassigned member. Accepting a split batch first checks draft
capacity and all source/target expectations, then atomically commits the selected
batch; failure leaves the batch unaccepted. Reviewer mode performs no agent
call. Exact copy uses precisely one eligible full body or finding suggested
comment under the 16 KiB bound. Agent mode dispatches only the selected immutable
set and produces an unaccepted agent-authored candidate; editing adds a reviewer
successor. The composition request variant has no publish/resolve/submit action.

Before an explicit Publish reaches the existing mutation host, the shared draft
owner must durably freeze the exact draft/composition revision, approved-prose
digest and target in a **pending publication join**. This row is correlation,
not a successful lineage record or a remote-attempt journal. It reserves local
capacity for the later terminal so ordinary quota growth cannot lose lineage.
Failure to commit it sends nothing. Map the existing coordinator's exact logical
key/fingerprint to this row before provider admission. Fresh durable publication
keys include ledger incarnation and stable draft revision alongside the existing
target/body identity, preventing database recreation or draft-ID reuse from
colliding with a previous ledger's publication. Keep existing persisted legacy
keys usable for recovery; RFC-0022 child keys and representation decisions stay
with that authority. BPR-0584 qualifies this narrow key extension and old rows.

No cross-database atomicity is assumed. The order is ledger join commit →
existing recovery-journal admission/transmission/receipt → successful server
reconciliation → ledger lineage/draft-terminal commit. Crash before the first
remote admission leaves a prepared local join with no authorization to post on
restart. A prepared/unresolved or possibly-transmitted remote journal continues
its existing read-only reconciliation rules. Receipt success with failed refresh
is still reconciliation-pending. Only the exact canonical logical receipt plus
reconciled server identity can materialize successful lineage. Repeated repair
is an idempotent local insert/update keyed by scope/logical publication; a
mismatched receipt/body/actor fails closed and does not search by text alone.
A failed local lineage commit after remote success is visible as local repair
pending; reload journal/refresh and retry only that local commit. Never run
`publish_attempt` or issue another POST to repair local state.

Store original draft/composition revision, expected and accepted head, exact
semantic and wire target, acting account snapshot, logical publication identity,
idempotency digest, approved-prose and wire-body digests, representation and
canonical `ServerCommentId`. Native wire body equals approved prose. File-level
wire body is RFC-0022's canonical visible permalink, blank line and approved
prose. No model, private source IDs, metadata or disclosure is injected. A child
native/fallback attempt is not a second local publication. Multiple deliberate
publications from the same sources use distinct draft identities; duplicate
source/body/target evidence is only an advisory.

### Current Qt navigation, focus and public composition

Keep the real Overview/Read/Review/Discuss modality controls and semantic drawer
intents. Overview opens new PRs and does not show drawers. In the conversation
tray, retain Shared (current Comments), Private (evolved Notes) and Tour roots;
these provide BPR-0585's equivalent Shared/Private navigation without merging
provider and ledger models. Each root retains its own stable selection and
scroll. Selecting a private subject in Read/Review reveals only its exact
available comparison; Discuss gives the selected conversation the central detail
surface. Switching modality or document tab never silently changes the thread
binding or selected composition sources.

Replace Notes' stacked note/launcher/chat/finding panels with one paged private
catalog and one selected-thread detail/transcript. New note and Ask actions
produce turns there; findings are a selectable source view with the existing
explicit actions. Run configuration and provenance are disclosures for the
selected operation/run. The persistent label reads “Private · stored on this
device · not visible on GitHub.” A shared conversation stays a GitHub read model;
a local thread resolving/archiving has no shared-resolve action. Model keys are
qualified shared or private IDs, never overlapping row ordinals.

Squash opens a dedicated centre composition page using
`open_review_composition`/`CompositionReturnFocus`/Back-or-Escape mechanics, with
source list, expandable history/provenance, exact target/split preview, candidate
body and three named modes. It reuses the retained draft editor after **Accept
as Draft**, keeping **Publish** as the existing separate action. Acceptance
focuses the local draft body or its status, never triggers/default-focuses a
publish button. Back/Escape returns to the originating source and stable reading
position; deleting that source uses a deterministic neighbour/header fallback.
No new conflicting global shortcut is allocated here: reuse existing window
composition actions and add standard focusable QAction/menu access for private
commands during BPR-0586's shortcut inventory.

Keyboard order is root selector → catalog → transcript/selected revision →
history/provenance/context selection → write/ask/squash actions. Within compose:
sources/details → mode/profile/settings → target/split → candidate → Accept →
Back. History/detail disclosures return focus to their invoker. Space toggles an
exact source; keyboard selection never automatically opts in an entire thread.
Each row announces local/shared, typed actor and basis, incomplete/stale state,
and selected-source state in text, not just color. Publish/Accept/local-resolve
have distinct accessible names. Emit one combined polite status per accepted
change. Qt applies insert/remove/move/data-change deltas and preserves selection;
no transcript or shared-model reset for one turn or provenance update. Long
bodies are virtualized/paged; source and provenance tables do not force the
drawer width. Scale/theme/native/font/focus evidence remains BPR-0589 work.

Profile composition injects the ledger executor/store service and shared draft
owner; window composition injects exact authority/window context, private and
composition consumers, projection sinks and callbacks into the existing chat,
invocation and mutation hosts. The profile service outlives every window's
admitted write/receipt callback. Opening a sibling window observes committed
state and uses CAS; it cannot own a second store policy or global selected
thread. Account replacement clears private projections before a new consumer
attaches. Constructor absence of a private store, run-admission seam, shared
persistent draft facet or lineage observer must make the relevant action
unavailable rather than report success; the public construction-cut course
proves each selected seam matters.

### Criterion/evidence ownership and review challenge

The existing eighteen-row matrix in [the verification ledger](RFC-BPR-0023.md)
remains canonical and all feature results remain Not run. The following cases
refine its direct evidence, without replacing any row or pending acceptance:

| ACs / owning cards | Required falsifier at the allocated seam |
|---|---|
| 01, 03, 04, 07 — BPR-0578/0579/0580/0582 | Replay exact/legacy binding, successor actor/DAG order and CAS; restart exact bytes; head move cannot retarget; source history remains inspectable. |
| 02, 05, 06 — BPR-0581/0582 | Existing invocation callback with two runs in one session; prompt/run commit failure means zero process calls; complete text differs from finding claim; metadata conflict/absence and session-prune/restart retain provenance. |
| 08, 10, 16 — BPR-0579/0582/0583/0586 | Three modes, historical selection, changed-tip reselection, cross-scope/missing/duplicate refusal, full-body exact-copy and explicit split/unassigned cases; compare actual child input to exact selected set. |
| 09, 11 — BPR-0580/0583/0587/0588 | Accept/split transaction and sibling CAS plus process kill/restart; inject all inline, whole-review, resolve and Tour mutation ports and assert zero calls before separate Publish. |
| 12, 13 — BPR-0584/0587/0588 | Native/file logical publication and exact prose/wire digests; remote success/local failure/failed refresh; repair and restart never POST. Designated target or signed recording plus independent body/actor read remains required. |
| 14, 15 — BPR-0580/0588 | Every v1–v6 origin, exact v5 and high-water v6/deleted highest note; canonical schema has no text-owning local_note; 1 MiB body two-chunk progress; each count/raw/escaped/wire bound±1 and no partial commit. |
| 17 — BPR-0580/0584/0588 | Unreferenced thread deletion, referenced deletion refusal and explicit dependency cleanup, scope purge and ledger recreation/incarnation; crash at each transaction/journal join; unchanged GitHub via independent read. |
| 18 — BPR-0585/0586/0587/0589 | Real window from Overview to Private, create/ask/restart/select/compose/accept/separate Publish; shared sync still works; keyboard/a11y and 100/125/200% themes, no clipping or bounded-update reset. |

The final M10/BPR-0591 independent reviewer must attempt counterexamples for source-use admission
versus edit/delete, immutable lineage versus cleanup, exact legacy migration,
draft single authority, account/window retirement, protocol fact overclaim,
input/body bounds and the two-store publication crash join. Self-review and the
documentation gate cannot satisfy that final gate. Under owner amendment r2,
one capability-critical independent review covers these architecture boundaries
and their implementation on the final integrated candidate before RFC closeout.
No reviewer has been dispatched by this leaf. M2 may be accepted and M3 started
on its architecture criteria and checks; the coordinator retains the authorized
final review obligation at M10/BPR-0591.

## M3 current-main reconciliation

While M3 was active on the M2 base, local `main` advanced to
`75102233` through the review-tour maintenance milestones. The inspected
`1f964cfa..75102233` range adds tour update/reconciliation/revision core values,
application staged maintenance, provider operations, and Qt/public courses.
The only overlapping M3 product paths are additive registrations in
`src/core/CMakeLists.txt` and `tests/core/CMakeLists.txt`; preserve both sets on
integration. New private-review, composition and revisioned-draft policy has
no tour dependency. The coordinator owns integration into the moving main.

The ledger schema is still 6 and mutation-recovery schema is still 5.
The staged-tour payload codec inside the recovery store advances from 2 to 3
for update plans; it is not a new database schema frontier. M4 must recheck
both live frontiers before allocating a version, and M7 must preserve that
staged-tour payload and the existing RFC-0022 logical-recovery authority while
adding its local publication join. No original criterion, dependency, plan
exit or owner review-timing amendment changes. The current M3 focused evidence
is for this branch candidate; coordinator integration must retain main's tour
registrations and run the directly affected combined checks.

## M4 live allocation and storage reconciliation

M4 starts from integrated main `2a254e638d364d249e79148db82b13d0fceccca5`.
The live frontiers remained ledger 6, mutation recovery 5 and staged-tour codec
3. BPR-0580 allocates **ledger schema 7** and preserves the other two authorities.
The focused recovery, submission-recovery and tour-store courses pass beside
the new private-store tests. No recovery/tour production codec was edited.

The M2 storage allocation is implemented in
`application/private_review_store.hpp` and the SQLite `private_review_store`
factory. Immutable turn/finding/candidate vertices share `private_content` with
scope-qualified identity, kind, digest and exact body/metadata. Their owning
headers, runs, edges, compositions, source sets, draft linkage, publication
join and legacy map remain separate STRICT tables. One universal high-water
frontier assigns all new durable identities and vertex orders in an atomic
bounded batch; the v6 note CAS high water survives independently. This is a
concrete decomposition of the allocated records, with no new workflow owner.

Two implementation refinements are required by the live connection policy:
private deletion uses explicit dependency order because the existing zero
trigger-depth setting refuses foreign-key cascades; the ledger alone raises
SQLite's row/string limit to 2 MiB so the required 1 MiB assistant body plus
metadata fits. Schema body bounds, page limits and both 64 MiB budgets remain
unchanged. The old note facade reads only the original mapped turn and every
canonical thread update advances its note CAS, preserving coherence while a
thread gains new turns. Run terminal persistence preserves its immutable
admitted source set after a source-tip advance; dispatch admission remains an
exact source/tip check.

The durable draft facet stores the existing `revisioned_review_draft` and
references linked candidate bodies. M7 still owns asynchronous shared-host
staging, persistence-before-fan-out, runtime-scope reattachment and publication
repair orchestration. Store tests are not public UI or remote-mutation evidence.
M9 retains native and designated-publication qualification, and M10 retains the
single final independent review under r2. All eighteen RFC criteria, authored
milestone exits and original imported source bytes are preserved. No change to
the adopted milestone order or whole-card obligation is needed.

## M5 protocol and input reconciliation

M5 starts from integrated main `fa2eefe9` and preserves ledger schema 7,
mutation-recovery schema 5 and staged-tour codec 3. It extends the live shared
protocol loop and process-launch boundary; no second launch path or provider
client is added. The current, field-level capability matrix is recorded in
[the agent boundary](../architecture/ai-agent-boundary.md#tested-protocol-capability-boundary).
Earlier source-audit protocol observations remain historical baseline evidence.

Only App Server has an implemented explicit requested/default setting transport
in this slice. Other protocols refuse these inputs; a profile name is never a
substitute. Reports retain their event/field basis, including the distinction
between App Server thread defaults and turn overrides. Unsupported facts stay
absent. The actual vendor tool can reject a bounded setting it does not support;
M5 does not claim model-catalog discovery or runtime execution attestation.

The source snapshot API carries M4's store-issued run identity, exact selected
references and full digest-checked bodies into the existing launch builder.
The builder's pre-encoding counter and the process adapter's independent
pre-start admission enforce the M2 8 MiB total wire budget, including App Server
follow-ups and reserved remote-ID escaping. A real child side-effect canary and
its small-input negative control distinguish refusal from a child that simply
did not report output. Legacy fake requests incorrectly put deferred context
on every protocol; they now give that field only to App Server, matching the
production builder. The scope and original acceptance are unchanged.

The adopted M5 exit is the durable run identity contract. M6/BPR-0582 owns its
production admission/terminal transaction wiring and complete assistant-text
retention, so zero-run legacy callers remain visible until that step. The new
public invocation-host course already exercises explicit settings through the
existing dispatch path, and the SQLite course stores decoded typed facts,
reopens them exactly and scans its owned files for ignored raw-envelope
credential canaries. These are direct protocol/storage evidence, not a claim
that AC-02/06/16 or the complete UI workflow is finished. All eighteen criteria,
authored plan exits and retained source-pack bytes remain unchanged. M9 native
and designated-publication obligations and the final M10 independent review
remain pending under r2.

### Current-main overlap discovered during M5 integration

While M5 was in progress, local main advanced from `fa2eefe9` to `ff43c68c`
with the scoped review-tour writing-assistance path (BPR-1007–1010). That
evolution adds a Claude-specific `WritingOnly` request and decoder policy on
the same invocation/launch surfaces M5 extends with requested settings,
run identity and runtime metadata. The integration preserves both: writing
restrictions and decoder checks stay in force while M5 metadata and input
accounting flow through the same builder and decoder. Focused writing-delivery,
launch-panel and child-canary tests passed on the combined tree. This changes no
RFC-BPR-0023 criterion or milestone order; M6 starts from the integrated main
tip and must preserve both paths.

## M6 invocation and retention reconciliation

M6/BPR-0582 starts at integrated main `09bb9a75`, with ledger schema 7,
mutation-recovery schema 5 and staged-tour codec 3. It adds the private run port
to the existing invocation host; the host remains the only workspace/process
owner. M8/BPR-0587 owns composition-root and UI connection of this port.
The scoped `WritingOnly` path remains separate, refuses a private-run intent,
and retains its Claude/tool/checkout/continuation restrictions.

The concrete host reloads exact selected source descriptors, checks source
revision/anchor authority, reads bounded full bodies, and atomically commits the
reviewer prompt and immutable run before workspace preparation or child launch.
After preparation it repeats source-tip/digest checks under a SQLite immediate
transaction, held through the initial or deferred private input acknowledgement.
The claim is cancelled through the invocation stop token and always releases
on write, refusal or terminal. SQLite excludes writers across the ledger while
that short claim exists; concurrent writes return typed `busy` after the existing
bounded busy wait. App Server's existing 120-second process limit bounds a peer
that never requests input. This deliberately coarse exclusion is not a claim of
per-source SQLite row locking. Store/claim ownership stays on the serialized
effect lane and the store must outlive the invocation.

**M6 lifetime clarification, accepted under mandate r2 before acceptance:** raw
selected-body vectors are dropped after encoding and before process launch. The
existing immutable launch request retains its bounded encoded input until
process retirement, including deferred App Server context. The acknowledgement
releases the source-write exclusion, not the request allocation. No new durable
supplement or canonical content buffer is introduced. This narrows the earlier
M2 sentence about releasing body buffers at input delivery to the actual shared
launcher lifetime; it preserves the 2 MiB selected-content/8 MiB wire bounds,
explicit selection, cancellation and acknowledgement guarantees. It changes no
RFC criterion, milestone exit or authored plan/source-pack bytes.

Terminal processing preserves validated assistant items once per protocol item,
with item byte boundaries in run provenance and a deterministic concatenation
in the assistant turn. Provenance's opaque record advances to version 2 for
those boundaries and still decodes version 1 with boundaries unknown. The SQLite
schema remains 7, and no body copy is added to the run record. Finding snapshots
retain their canonical identity and full structured fields; readable selected
finding input has a separate labeled serialization checked against that exact
snapshot. The selected-input bound covers that rendered size too.

Correlated original output is persisted before stale navigation presentation.
Explicit cancellation wins over a late success return; incomplete output cannot
become eligible. Wrong run/session, malformed metadata, oversized output or
malformed finding data produces a protocol failure, with no false complete
turn. Duplicate terminals conflict. A terminal storage refusal remains visible
as `private_storage_failure`; recovery marks outstanding admitted runs
interrupted after producers quiesce, without rerunning. If an explicit archive,
delete, purge or conflicting edit makes the response transaction ineligible,
that transaction refuses rather than reopening or resurrecting the thread.
This follows the same storage-failure/recovery boundary.

The [M6 evidence](RFC-BPR-0023.md#m6-private-thread-application-host) binds the
new SQLite/invocation course and real child canary. It establishes host/store
reachability, not finished application UI or installed-provider qualification.
M7 composition/publication, M8 UI, M9 native/designated publication and the one
M10 independent review remain allocated as before. All eighteen criteria remain
intact.

### Current-main overlap discovered during M6 integration

Main advanced during M6 from `09bb9a75` to `2e2aca96` with review-tour
milestones BPR-1011/BPR-1012, including scoped-writing parser/configuration
adversaries and related end-to-end checks. The changed agent launch path
auto-merged with M6's run admission/terminal work. The coordinator rebuilt and
ran the private host/storage, invocation/decoder, writing-delivery, child
canary and architecture contract courses on the combined tree; all ten rows
passed. M6's separate `WritingOnly` path and M5 provenance remain in the
integrated source. No RFC-0023 criterion or authored milestone order changed;
M7 starts from this integrated main.
