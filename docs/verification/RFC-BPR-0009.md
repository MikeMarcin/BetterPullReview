# RFC-BPR-0009 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: implementation in progress. The core defect lifecycle, its evidence
policy, host correlation, the Qt defect card, and anchor history have landed
(BPR-0146 through BPR-0150). RFC-BPR-0009 stays `Proposed`.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0008, which is implemented on available targets and
stays `Proposed` behind its three owner-supplied gates and the ordered closure
chain. Done BPR-0088 records the product owner's direction to advance the
implementation frontier on available targets.

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
| Only an explicit reviewer intent verifies | Core, host, and Qt | Attempt to verify from each layer without the intent | No host correlation and no widget action can produce `verified` | Verified in all three layers, each asserting it separately rather than inheriting. BPR-0149 presents every defect state, triggers every action, and shows the card's own state never moves |
| Classifying mutates nothing shared | Host | Classify an existing thread | No request is issued until a distinct shared action is taken | Verified. BPR-0148's correlator is a pure function with no provider, so there is nothing here that could issue a request; mutation goes through RFC-BPR-0008's coordinator, reached by a separate action |
| Reopening is a separate explicit action | Core and host | Reopen a verified defect | Reopening never rides along with another action and may unresolve the thread only when asked | Planned |
| An outdated anchor is honest | Core | Remove the anchor a defect was filed against | The original context and state are retained and the unavailability is explicit, never repaired with a nearest-line guess | Verified. BPR-0150 keeps the original anchor and the lifecycle state through both ways of losing context, and the type has no field for a substitute anchor — the absence is the guarantee, since there is nothing to set |
| Counts and navigation agree | Core plus Qt | Compare the queue to local state and thread status | The presented counts follow the projection rather than a separately maintained tally | Partially verified. BPR-0149's card derives its badge, explanation, and every action's enablement from the presented projection and holds no state of its own, so there is no separate tally to disagree; a queue view aggregating many defects is not built |
| The published prose stands alone | Documentation plus fixture | Read a published defect without local metadata | Severity and category enrich the local UI only; the comment is comprehensible without them | Planned |

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

## Closeout blockers

RFC-BPR-0009 cannot become `Auto-Verified` until RFC-BPR-0008 is, and therefore
not before that RFC's three owner-supplied gates and the whole ordered chain
back to RFC-BPR-0002's Windows evidence. It additionally cannot close before
BPR-0153. BPR-0152 will make no closure claim.
