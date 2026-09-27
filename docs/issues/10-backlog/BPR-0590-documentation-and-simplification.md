---
schema: "implement-rfc.card.v1"
id: "BPR-0590"
title: "Update product/architecture/operator documentation and simplify the implementation"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0588","BPR-0589"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Update product/architecture/operator documentation and simplify the implementation

## Objective

Make the final authority, limits, privacy, provenance, and workflow understandable and remove accidental complexity before closeout.

## Required final state

- All documentation impacts named by the RFC are updated or carry evidence-backed no-impact entries.
- Operator documentation explains Private, model/effort qualifications, revision history, Squash to Draft, Split into Drafts, separate Publish, stale states, deletion, and public-body behavior.
- Developer/agent documentation identifies core/host/Qt ownership, store/protocol ports, async identity, migration, limits, crash behavior, and test commands.
- A conservative simplification pass removes duplicated policy, broad service access, dual representations, unnecessary serialization, and whole-state refreshes.
- Documentation and focused tests are rerun after simplification; behavior and evidence digests remain valid.

## Non-goals

- Rewriting unrelated documentation
- Collapsing semantic distinctions for fewer types/tables
- Changing public scope during cleanup

## Completion evidence

- Documentation ledger
- Documentation gate output
- Simplification diff/review
- Post-simplification focused test output

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
