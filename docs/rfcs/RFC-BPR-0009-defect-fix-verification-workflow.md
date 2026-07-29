---
id: RFC-BPR-0009
title: Defect Classification, Fix Claims, and Verification
status: Proposed
area: BPR
milestone: M3 Collaborative Round Trip
depends_on: [RFC-BPR-0008]
pack_version: 2
---

# RFC-BPR-0009: Defect Classification, Fix Claims, and Verification

## Summary

Restore inspection-style defect semantics as a local projection over ordinary GitHub review threads, separating author fix claims from reviewer verification.


## Motivation

CodeCollaborator-style defects distinguish required corrective work from ordinary discussion and preserve a separate verification step. GitHub review threads can carry the shared conversation, while BetterPullReview adds the personal inspection lifecycle.

## Goals

- Classify ordinary GitHub review threads as local defects.
- Track severity, category, open/fix-claimed/needs-verification/verified/reopened dispositions.
- Detect relevant author replies and code changes as verification candidates.
- Keep author fix claims distinct from reviewer verification.
- Present defect queues and navigation without requiring collaborators to use BetterPullReview.

## Non-goals

- Creating custom GitHub issue types, labels, bots, or checks.
- Automatically claiming a defect is fixed or verified.
- Hiding defect meaning in metadata unavailable to GitHub-only users.

## Architectural allocation

- **Core:** owns defect classification and legal transitions among open, fix-claimed, needs-verification, verified, reopened, withdrawn, and accepted-as-is. A nearby code change is evidence, not an automatic transition to verified.
- **Host:** correlates ordinary GitHub threads/replies/epochs with local defect records, persists local classification, and performs normal GitHub reply/resolve requests emitted by the core.
- **Qt adapter:** presents defect cards, state badges, evidence, and explicit reviewer actions beside code; it does not infer or commit lifecycle state on its own.
- Shared meaning remains in normal GitHub prose and thread state, while richer private projection follows deterministic core policy.

## Shared mapping

Publishing a defect creates an ordinary actionable review comment. Replies remain ordinary replies. A user can optionally post normal prose such as `Fixed in <commit>`. Verification normally corresponds to a reviewer reply and/or GitHub thread resolution. Rejection keeps or reopens the ordinary thread.

Local classification may enrich the UI with severity and category, but the published prose must stand on its own.

## Acceptance criteria

- A GitHub-only collaborator can understand and respond to every published defect.
- Classifying an existing thread does not mutate GitHub until the user performs a distinct shared action.
- Nearby code movement or an author response creates `Needs verification`, never `Verified`.
- Reviewer verification is explicit and optionally resolves the GitHub thread.
- Reopening a verified item can unresolve the thread through a separate explicit action.
- Deleted/outdated threads preserve local audit history while clearly showing unavailable shared context.
- Defect counts and navigation agree with local state and current GitHub thread status.
- Defect lifecycle transitions are core-tested without GitHub/Qt, and integration tests prove host correlation plus Qt presentation cannot auto-verify a defect.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Fix is not verification | Real workflow | Push nearby change and confirm only needs-verification state |
| GitHub-only meaning is intact | Independent client | Read defect prose and reply without local metadata |
| Verify/reopen mappings are explicit | UI and mutation fixture | Exercise local-only and shared resolve/unresolve actions separately |
| Outdated anchor is honest | Historical fixture | Remove anchor and inspect retained original context/state |
| Defect policy is core-owned | Transition and adapter tests | Host/Qt cannot mark verified without the explicit semantic reviewer intent |

## Documentation impact

Defect semantics, state machine, GitHub mapping, severity/category privacy, verification rules, anchor history, and user-facing action labels are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
