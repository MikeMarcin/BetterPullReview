---
provisional_id: PACK-BPR-0022-08
title: Implement composition workspaces and non-destructive Squash to Draft
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-05, PACK-BPR-0022-07]
---

# Implement composition workspaces and non-destructive Squash to Draft

## Objective

Turn exact private sources into a durable local draft through manual, copy, or explicit agent-assisted composition without mutation.

## Required final state

- Composition admission snapshots exact source IDs/digests/provenance/staleness and target partition.
- Reviewer-authored and exact-copy candidates work without an agent.
- Agent-assisted squash uses the one invocation host, explicit selected context, and one new run; returned candidate remains a proposal.
- Editing creates reviewer-authored successor composition revisions with immutable derivation.
- Accepting a candidate atomically persists accepted composition and creates/updates one durable RFC-BPR-0008 draft with exact linkage.
- Multi-target selection requires exact target selection or Split into Drafts; unanchored sources are not guessed.
- Source threads/findings remain unchanged and traceable after acceptance.
- Instrumentation proves every mutation capability receives zero calls for create/edit/squash/accept/restart.

## Non-goals

- Publishing or resolving
- Automatic AI squash
- PR-level mutation target invention outside RFC-BPR-0008

## Completion evidence

- Composition core/host tests
- Agent-assisted selected-input trace
- Durable draft restart test
- All-mutation-port zero-call canary
- Source retention/digest comparison

## Documentation impact

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
