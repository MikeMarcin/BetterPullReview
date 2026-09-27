---
schema: "implement-rfc.card.v1"
id: "BPR-0585"
title: "Implement coherent private/shared conversation navigation and incremental private transcript presentation"
status: "In Progress"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0578","BPR-0579","BPR-0580","BPR-0582"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Implement coherent private/shared conversation navigation and incremental private transcript presentation

## Objective

Present durable private deliberation alongside, but never confused with, ordinary shared GitHub conversations.

## Required final state

- Conversation navigation offers coherent All/Shared/Private access or an equivalent reviewed design.
- Every private surface persistently states local/not-visible-on-GitHub status.
- Thread lists and transcripts use stable-identity incremental models over paged observations.
- Turn cards show typed actor, completion, stale state, composition/publication badges, and revision-history affordance.
- Provenance compact labels remain honest when model/effort is unknown; details expose all fact bases and conflicts.
- One turn insert/edit/completion does not reset the entire transcript or shared GitHub conversation model.
- Qt emits semantic intents only and has no SQLite/agent/GitHub effect access.

## Non-goals

- Copying GitHub shared records into private storage
- One giant all-review transcript load
- Final composition editor behavior

## Completion evidence

- Qt model/delegate/widget tests
- Model reset/change statistics
- Private/shared authority presentation review
- Forbidden include/effect boundary check

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
