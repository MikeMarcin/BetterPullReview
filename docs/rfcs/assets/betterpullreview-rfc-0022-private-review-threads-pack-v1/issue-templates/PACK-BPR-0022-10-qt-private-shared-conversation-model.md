---
provisional_id: PACK-BPR-0022-10
title: Implement coherent private/shared conversation navigation and incremental private transcript presentation
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-03, PACK-BPR-0022-04, PACK-BPR-0022-05, PACK-BPR-0022-07]
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
