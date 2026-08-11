# RFC-BPR-0009 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **the policy, durable authority, and shipped defect workflow are
composed.** BPR-0146 through BPR-0150 delivered the lifecycle, evidence policy,
correlator, Qt card, and anchor history. BPR-0238 supplied the schema-versioned
identity-scoped durable authority. BPR-0230 now connects accepted ordinary
threads and real selected comparisons to that authority and presents a durable
multi-defect queue through the shipped workbench.

RFC-BPR-0009 stays `Proposed`. No card here makes a closure claim, and one
acceptance criterion remains blocked on a designated repository and a person.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0008, which remains incomplete and `Proposed` behind
its product-composition, isolation, owner-supplied, and ordered-closeout gates.
Done BPR-0088 records the product owner's direction to advance the
implementation frontier despite that open work.

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
require the asynchronous application-host and coordinator constructors.
Development fixture additionally requires the exact test-only provider,
testing-grant factory, and `bpr::application::reconcile`, while forbidding the
live provider and factory. Tests-disabled production requires the live provider
and factory while forbidding the fixture type, testing-grant factory, and
reconciliation symbol. Independent construction, grant, selection, and
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

## Deferred gates

| Card | Gate | What is missing |
|---|---|---|
| BPR-0153 | A GitHub-only collaborator can understand and respond to every published defect | The designated repository and token from BPR-0142, plus a person reading the thread as a collaborator who has never used this tool |

The "real workflow" phrasing in the RFC's own matrix for *fix is not
verification* is satisfied on this host: pushing a nearby change is modelled as
evidence, and the policy that evidence cannot verify is core-testable. What
needs the live target is only whether a GitHub-only reader finds the published
prose sufficient.

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

## Closeout blockers

RFC-BPR-0009 cannot become `Auto-Verified` until RFC-BPR-0008 is, and therefore
not before that RFC's three owner-supplied gates and the whole ordered chain
back to RFC-BPR-0002's Windows evidence. It additionally cannot close before
BPR-0153. BPR-0152 will make no closure claim.
