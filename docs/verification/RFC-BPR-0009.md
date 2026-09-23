# RFC-BPR-0009 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **Auto-Verified under owner amendment r4.** The policy, durable
authority and shipped defect workflow are composed. BPR-0146 through BPR-0150
delivered the lifecycle, evidence policy,
correlator, Qt card, and anchor history. BPR-0238 supplied the schema-versioned
identity-scoped durable authority. BPR-0230 now connects accepted ordinary
threads and real selected comparisons to that authority and presents a durable
multi-defect queue through the shipped workbench.

Owner amendment r4 narrows terminal acceptance to the implemented scope.
BPR-0243 is Done; BPR-0153 remains a standalone parked follow-up for
the unobserved collaborator-comprehension/reply criterion. The amendment does
not count that human observation as passed.

## Predecessor and historical implementation authority

RFC-BPR-0008 is Auto-Verified under its own r4 amendment at entry revision
`02e419b8d878620c55447ddb3d5d53683dca3f91`. Its separate live/human follow-ups
remain pending. Earlier implementation proceeded under Done BPR-0088's owner
direction while RFC-0008 was still Proposed; that historical exception is no
longer needed for this closeout. Canonical ordering is preserved.

## Owner-amended closeout r4 — 2026-09-21

The coordinator's exact assignment is to close only RFC-0009 under the owner's
direction to land RFC-0014, deferring genuine unfinished human/external work
durably. The [mandate](../rfcs/plans/rfc-bpr-0009-owner-closeout/mandate-r4.md)
and [adopted plan](../rfcs/plans/rfc-bpr-0009-owner-closeout/milestone-plan.json)
bind that interpretation to the immutable baseline above. No product semantics,
source code, test assertions, protected review artifacts or historical Done
cards change in this milestone.

The original first criterion, "A GitHub-only collaborator can understand and
respond to every published defect," remains unobserved. The live RFC now
accepts the implemented ordinary-prose/private-enrichment boundary and names
BPR-0153 as its standalone follow-up. BPR-0153 retains its exact ID, parked
path, original evidence requirements, unfamiliar collaborator, concrete
defect/disposable PR and separately authorized publication/reply course.
Existing sign-in is retained; BPR-0142's prior round-trip comment is not a
suitable defect and no new token is requested. No contact or publication is
authorized by this transfer. A future attended session requires those inputs;
no review-ready candidate, new server, session or human verdict is claimed now.

The remaining seven RFC criteria are unchanged. Local Verify/Reopen do not
resolve/unresolve a shared thread as a side effect. The current conversation
actions issue separate typed gestures; core capability/identity checks and the
mutation host govern them. RFC-0008's BPR-0143 retains supported live capability
round trips. This RFC's local evidence does not certify those observations.

### Exact candidate and verification

Product and test source candidate:
`02e419b8d878620c55447ddb3d5d53683dca3f91`. All changes in this closeout are
documentation/tracker changes, so the passing source-bound evidence remains
applicable to the terminal documentation commit. Builds of the real application,
defect core/correlation/card, review-ledger host/storage, workbench window and
review-mutation host targets all reported `ninja: no work to do`.

| Command or inspection | Result | Acceptance and limit |
|---|---|---|
| `ctest --test-dir build/dev --output-on-failure -R '^application_defect_composition_contract$'` | Pass, 1/1, 52.91 s | Actual application, GitHub.com and GHES 3.18 fixture seed/reopen. Exact two-defect identity, evidence ceiling, explicit Verify, private writes, unavailable original anchor, count/navigation and restart are required. Expected evidence is `11111110011` for seed and `11111111110` for reopen. No live service is contacted. |
| `ctest --test-dir build/dev --output-on-failure -R '^(core_defect_workflow\|application_defect_correlation\|application_review_ledger_host\|sqlite_review_ledger_storage\|qt_defect_card\|qt_workbench_window_scale_100)$'` | Five pass, one fails, 2.77 s | Core, correlation, ledger host, SQLite and defect card pass. The broad workbench suite fails before its defect checks at the already tracked BPR-0940 epoch-row assertion; it is not passing evidence. |
| `ctest --test-dir build/dev --output-on-failure -R '^application_review_mutation_application_host$'` | Pass, 1/1, 0.13 s | Explicit thread-resolution capability/grant admission and typed completion, including refusals. No live resolve/unresolve result is inferred. |
| Source/type inspection at the product candidate | Pass for the structural boundary | `review_publication_proposal` and `review_mutation_request` carry ordinary body/anchor/target/actor data, with no defect severity/category fields. The pure correlator delegates all evidence to core policy. Defect-card lifecycle commands and ordinary conversation resolve/unresolve gestures are distinct. |
| RFC-0008 r4 core/coordinator and public round-trip evidence | Reused with unchanged source | Prior entry's exact ordinary comment/reply, core `decide_thread_resolution`, publication safety and fixture publication records remain applicable; BPR-0143/0144/0641 remain pending as recorded there. |

The broad-window failure is exactly `the epoch row is the first row of the
client area and as wide as it`, already preserved by
[BPR-0940](../issues/30-in-progress/BPR-0940-reconcile-composed-github-legacy-assertions.md).
Its prior r23 control failed with the old comparison status restored, and the
new run fails in the same epoch-row setup before defect navigation. No new
defect-flow failure was observed. The independent actual-application course
above directly requires two defects, a `/2` count label, navigation between
different stable identities, and exact restart state. That passing course
supplies the current count/navigation evidence; the broad suite remains failed
and BPR-0940 remains open under RFC-0013. No test is disabled or weakened.

The scoped final-state inventory was compared with the immutable RFC base:
defect core/header, correlator/header, Qt card/header, their three suites and
the real composition course are present, with shared application/workbench and
ledger integration retained. Existing implementation and later anchor/privacy
corrections remain intact. This is an implementer self-review, not independent
acceptance. Documentation structure and verifier regressions are final
closeout checks recorded with the terminal result below.

## What this RFC is, in one sentence

Inspection-style defect semantics as a **local projection over ordinary GitHub
review threads**, with the author's claim that something is fixed kept strictly
separate from a reviewer's verification that it is.

## The rule everything else serves

**A nearby code change is evidence, not a verdict.** Code moving near a defect,
or an author replying to say they fixed it, produces `needs verification` and
never `verified`. Only an explicit reviewer intent verifies.

This is the criterion most likely to be eroded by a convenience — an
auto-verify for "obviously fixed" defects would be a plausible feature request
and would destroy the distinction the RFC exists to restore. It is tested in
the core, and the host and Qt slices must each prove they cannot reach
`verified` on their own.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Classification, the lifecycle among open, fix-claimed, needs-verification, verified, reopened, withdrawn, and accepted-as-is, and what evidence may and may not do | Qt-free and provider-free replay tests under `core-only` |
| Application host | Correlating ordinary threads, replies, and epochs with local defect records, persisting local classification, and emitting the ordinary GitHub requests the core proposes | Fake-provider and fake-store tests, including the proof that no host path reaches `verified` |
| Qt adapter | Defect cards, badges, evidence, and explicit reviewer actions | Adapter tests asserting every action is an intent and that no widget path reaches `verified` |

Shared meaning stays in ordinary GitHub prose and thread state. Severity and
category are local enrichment and must never be where the meaning lives: a
GitHub-only collaborator reads the prose and understands the defect.

## Claim matrix

A row is claimed only when its card lands with direct evidence.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| The lifecycle is decided in the core | Core | Drive every transition and every illegal one | Each refusal carries its own reason; the suite runs under `core-only` | Verified. BPR-0146 drives the ordinary lifecycle end to end and refuses verifying nothing, reopening what was never verified, commanding a settled defect, and reclassifying nothing — each with its own reason |
| A fix claim is not a verification | Core | Apply author reply and nearby-change evidence | Both reach `needs verification`; no evidence path reaches `verified` | Verified. BPR-0147 drives all three evidence kinds from all three receiving states — nine cases — and none reaches `verified`. An externally resolved thread, which is what a verification looks like from outside, reaches `needs_verification` like everything else |
| Only an explicit reviewer intent verifies | Core, host, and Qt | `ctest --preset dev -R '^(core_defect_workflow\|application_defect_correlation\|qt_defect_card\|application_defect_composition_contract)$' --output-on-failure` | No host correlation or widget presentation can produce `verified`; the public trace reaches it only through Verify | Verified. Component tests keep evidence and presentation projection-only. BPR-0230's GitHub.com/GHES trace observes author-reply and provider-outdated exact-line evidence as `needs_verification`, proves retained-epoch browsing leaves the unaffected defect open, then reaches `verified` for exactly the evidence-bearing defect through the real card Verify action |
| Classifying mutates nothing shared | Host and shipped workbench | `ctest --preset dev -R '^(application_defect_composition_contract\|application_review_round_trip_composition_contract)$' --output-on-failure` | Classification records two private defects with zero provider submissions; a separate Publish action is the shared path | Verified. BPR-0230 drives the real file and conversation selections and records zero fixture mutation submissions across classification, evidence, verification, and reclassification. Its source guard requires the distinct Publish action; BPR-0229's paired public contract proves that action reaches the fixture-safe coordinator |
| Local classification persists | Host store and shipped workbench | `ctest --preset dev -R '^(application_review_ledger_host\|sqlite_review_ledger_storage\|application_defect_composition_contract)$' --output-on-failure` | Severity, category, lifecycle, and retained anchor history survive restart without entering GitHub shared state | Verified. BPR-0238 supplies schema-v3 identity, conflict, migration, bound, and crash guarantees. BPR-0230 persists two accepted ordinary line threads under both halves of a SHA-256-derived identity, keeps one open and reclassified, explicitly verifies the other while retaining its outdated original anchor, then reopens the application with exact state and zero private writes |
| Reopening is a separate explicit action | Core and host | Reopen a verified defect | Reopening never rides along with another action and may unresolve the thread only when asked | Verified. BPR-0152 drives every other command and all three evidence kinds from every state and none reaches `reopened`; the Qt card offers reopen only from `verified`. Unresolving the shared thread is a separate mutation through RFC-BPR-0008's coordinator, which this RFC's code cannot reach |
| An outdated anchor is honest | Core and shipped workbench | Run `core_defect_workflow`, `qt_defect_card`, and `application_defect_composition_contract` | The original context and state are retained and unavailability is explicit, never repaired with a nearest-line guess | Verified. BPR-0150 supplies the structural no-substitute rule. BPR-0230 projects the fixture's outdated thread as unavailable, retains exact current/original anchors through restart, and keeps its evidence-decided lifecycle unchanged |
| Counts and navigation agree | Shipped Qt workbench | Run `qt_workbench_window_scale_100` and `application_defect_composition_contract` | The exact queue count and previous/next selection follow stable projected defect identities across restart | Verified. The window sorts the bounded histories by stable defect ID and retains selection by ID. Component and two-process traces both require two defects, `1/2`/`2/2` navigation, different selected identities, and exact reopen state |
| The published prose stands alone | Documentation plus fixture | Read a published defect without local metadata | Severity and category enrich the local UI only; the comment is comprehensible without them | Structurally verified. Neither `review_publication_proposal` nor `review_mutation_request` has a field for severity or category, so no publication path can carry them — the absence is the guarantee. Whether the remaining prose is *sufficient* for a GitHub-only reader is a judgement no fixture settles, and is BPR-0153 |

## Standing composed-host linkage audit

BPR-0248 requires the exact demangled
`bpr::application::correlate` decision anchor in both the
development-fixture and production binaries. This proves linkage only;
`application_defect_composition_contract` remains the evidence that the real
selected-thread path invokes correlation and reaches explicit verification
through the shipped workbench.

The review-mutation linkage inventory is profile-specific. Both profiles
require the asynchronous application-host and coordinator constructors plus
the generic `bpr::application::reconcile` decision used after mutation.
Development fixture additionally requires the exact test-only provider and
testing-grant factory while forbidding the live provider and factory.
Tests-disabled production requires the live provider and factory while
forbidding the fixture component type and testing-grant factory. Independent
construction, grant, selection, and
host-dispatch cuts are the runtime complements; production's empty grant
performs no live observation or submission. Inspector absence, nonzero
or empty output, demangling failure, and malformed or zero-defined symbol tables
are hard failures. Only parsed defined records count; `U`/`UND` references and
anchors embedded in surrounding template text do not. Windows declaration
prefixes are accepted only through the first allowlisted calling convention
outside balanced template, parameter, and array delimiters; nested conventions
do not select the anchor boundary and unbalanced declarations fail closed.
Absolute, common, debug, and unknown symbol kinds or sections do not count.
CTest treats the verifier's exit status as authoritative.

## Standalone follow-up

| Card | Gate | What is missing |
|---|---|---|
| BPR-0153 | Original collaborator-comprehension/reply criterion; outside terminal scope under r4 | A concrete defect/disposable PR, an owner-identified person who has never used this tool, and a separately authorized publication/reply course. Existing sign-in is retained; no new token request. Still Parked and unobserved. |

For *fix is not verification*, nearby change and author reply are exercised as
typed evidence in the core/host and shipped fixture course. No live push is
claimed. BPR-0153 owns the distinct question of whether a GitHub-only reader
finds the published prose sufficient and replies without local metadata.

## Card allocation

| Card | Slice |
|---|---|
| BPR-0145 | This planning slice |
| BPR-0146 | Core defect classification and the lifecycle state machine |
| BPR-0147 | Core verification evidence: what it may and may not conclude |
| BPR-0148 | Host correlation of threads, replies, and epochs with defect records |
| BPR-0149 | Qt defect cards, badges, evidence, and explicit reviewer actions |
| BPR-0150 | Outdated and deleted anchors, and retained audit history |
| BPR-0151 | Documentation |
| BPR-0152 | Simplification and closeout preparation |

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Defect semantics, state machine, GitHub mapping, severity and category privacy, verification rules, anchor history, user-facing action labels | [Defect Workflow](../architecture/defect-workflow.md) | Written. Records ownership, the typed command/decision/observation/delta vocabulary, the state machine, the three evidence kinds, the shared mapping, the privacy rule, anchors and history, and the badge and action labels — each checked against the source rather than from memory |
| Shared review round trip | [Review Round Trip](../architecture/review-round-trip.md) | No impact. Defects are a local projection over the threads that document describes; the shared protocol is unchanged and no new GitHub surface is introduced |
| Private review ledger | [Review Ledger](../architecture/review-ledger.md) | Updated by BPR-0238. Defect meaning remains a core/local projection, while schema v3 supplies its private durable table in the same review ledger authority, keyed by scope, defect, immutable file/conversation/current/original anchors, and mutable classification/lifecycle/availability; it shares no receipt row and carries no GitHub prose |
| Shipped defect composition | [Defect Workflow](../architecture/defect-workflow.md), [System Architecture](../architecture/system-architecture.md), root README | Updated by BPR-0230. Records exact accepted-thread/selected-diff admission, persist-before-project ordering, evidence-only correlation, stable multi-defect navigation, unavailable context, restart behavior, and the separate shared-action boundary |
| Owner-amended closeout r4 | Live RFC, BPR-0243, BPR-0153, this ledger, [Defect Workflow](../architecture/defect-workflow.md), RFC index and root README | Acceptance transfer and exact current evidence are explicit. No impact on APIs, state authority, provider/authentication behavior, persistence/schema, concurrency, UI commands, build/package, security or launch behavior because no product/test source changes. |

## Closeout disposition

RFC-BPR-0008's predecessor gate is clear. BPR-0243 is Done at
`d859d3df9cbee8c88ebfd51cd32d3d78f9c03eb9` with eight passing amended
criterion records bound to product/test source `02e419b8`. The owner amendment
and evidence landed at `0474e55a`; the guarded claim was `fa9f868f`.
RFC-BPR-0009 and the canonical index now agree on Auto-Verified.

The documentation gate, board validation and `git diff --check` pass; all 64
documentation verifier regressions pass in 2.113 seconds. Seven focused
product tests pass across the public composition and component/host runs;
the eighth, broad `qt_workbench_window_scale_100`, fails exactly as already
tracked by BPR-0940. No product/test source changed. No full-suite, native
package, live service, independent-review or release acceptance is claimed.

BPR-0153 remains a pending standalone follow-up, and BPR-0940 remains an
explicitly failed broad layout course outside this milestone. Neither is newly
accepted. BPR-0152's historical no-closeout statement remains true for its
dated implementation slice; BPR-0243 is the current closeout authority.
RFC-BPR-0010 is the next canonical scope for a fresh coordinator assignment.
