# RFC-BPR-0023 live audit and execution amendment

BPR-0576 / milestone `m1-live-audit` / mandate `r1`, 2026-09-26.
Source and tracker: `/Users/jaeger/.codex/worktrees/15a6/BetterPullReview`;
branch `codex/rfc-bpr-0023`. The coordinator owns main integration.

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
BPR-0577's detailed signatures, transaction choices and navigation allocation,
including its required independent authority-boundary review. That review is
pending and cannot be self-certified. Subsequent owners claim only eligible
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
| `agent_launch.hpp/.cpp`: decoder | App Server tracks remote session/thread/turn; JSONL tracks remote thread; Claude tracks remote session. These are decoder state, not exported provenance. | Surface supported bounded typed metadata with basis; do not turn framing IDs into authentication/model proof. BPR-0581. |
| Same files: `agent_run_result` | Result contains Session/Outcome/Events/stderr/exit/protocol-failure/findings, no distinct durable run or model/effort/version facts. Protocol output events exist. | Capture complete assistant bytes from validated output/terminal framing, not concatenated finding claims or raw log persistence. BPR-0581/0582. |
| `agent_chat_session_host.hpp/.cpp` | One existing invocation path remains. Chat now retains up to 16 in-memory `agent_chat_reply` rows assembled from validated finding claims; oldest reply is dropped. No prompt or complete-response transcript is durable. | Preserve current session policy while committing prompts/run admission before dispatch and retaining complete correlated responses. Temporary reply IDs are not durable turn/run IDs. BPR-0582. |
| `agent_finding.hpp` | Structured finding still retains provider, analyzed revisions, anchor, claim/evidence/consequence and suggested comment. | Snapshot exact validated data under run + ordinal + digest. Never infer identity from session + ordinal. BPR-0579/0582. |
| `agent_finding_host.hpp/.cpp` | Current delivery and explicit promotion remain; note promotion is revision-aware but still flattens source information. | Reuse explicit actions, retain source snapshot/lineage and connect workshop acceptance to the one draft authority. BPR-0582/0583. |
| `review_mutation_port.hpp` and `review_mutation_recovery_store.cpp` | ServerCommentId still exists; recovery journal is now **v5**, preserving ordinary and whole-review attempts plus RFC-0022 logical native/file representation recovery and durable public body. | Consume the logical canonical receipt, exact approved/wire digests and reconciliation; local lineage repair must never POST. BPR-0584. |
| `src/adapters/qt/src/side_chat_panel.cpp` | Recent reply list and promotion exist, but `present` clears/rebuilds this small temporary list when replies change. | Replace/bridge with stable-identity paged transcript updates; bounded temporary list behavior is not RFC-0023 incremental transcript evidence. BPR-0585/0586. |
| `src/adapters/qt/src/workbench_window.cpp` | Overview/Read/Discuss/Tour modalities coexist with Comments/Notes/Tour tray tabs. Notes holds private notes/agent/chat/findings; the draft and workshop have separate center composition pages. New PRs open on Overview. | Allocate coherent private/shared navigation against this UI; enter the appropriate modality before evidence interactions. Preserve exact selected tab/comparison and avoid another stacked transcript panel. BPR-0577/0585–0587. |

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
implementation; its independent review and BPR-0591's final independent review
remain separate required observations.

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
and 256 KiB selected source bytes; the decoder has a 64 KiB frame and 1 MiB total stdout ceiling. These are output
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
| Codex App Server | Decoder-owned remote session/thread/turn and terminal status. | No exported typed provenance. | Export supported IDs/facts from admitted envelopes; prove complete/partial/conflicting/absent and malformed framing. |
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
ready and therefore starts no review session. BPR-0577 and BPR-0591 independent
review are still pending; this leaf does not claim independence for its own audit.

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
