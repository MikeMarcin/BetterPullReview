---
id: BPR-0582
title: Implement the private thread application host and durable side-chat/finding ingestion
status: Backlog
rfc: RFC-BPR-0023
milestone: Post-Pack v2
depends_on: [BPR-0580, BPR-0581]
---

# Implement the private thread application host and durable side-chat/finding ingestion

## Objective

Coordinate deterministic private-thread decisions with durable storage and the existing agent invocation host.

## Required final state

- Reviewer prompts and run admission commit atomically before process dispatch; persistence failure launches nothing.
- Side chat dispatches through the existing invocation host and complete correlated responses become agent-authored turns.
- Cancellation/failure/timeout/stale/duplicate/late completions produce typed outcomes and cannot create false complete turns.
- Validated findings are snapshotted under run + ordinal + digest and can be linked into threads without lossy anonymous promotion.
- Private source selection carries only exact IDs; the host reloads/revalidates them at dispatch and refuses missing/edited/duplicate/cross-review/over-limit sources.
- Session catalog pruning leaves referenced run provenance readable.
- Bounded deltas update only changed thread/turn/provenance identities.

## Non-goals

- A second agent process port
- Implicit whole-thread context inclusion
- Automatic follow-up or rerun

## Completion evidence

- Fake store/process/manual executor tests
- Ordering trace proving commit-before-launch
- Reordering/cancellation/staleness matrix
- Private-context child-input canary
- Session-prune restart test

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
