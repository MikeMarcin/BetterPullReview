---
schema: "implement-rfc.card.v1"
id: "BPR-0590"
title: "Update product/architecture/operator documentation and simplify the implementation"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0588","BPR-0589"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Documentation impacts","outcome":"pass","revision":"eb7d72b61","evidence":{"kind":"artifact","detail":"RFC documentation-impact paths updated and reconciled; no-impact entries have M8/M9 evidence and open limits; docs/verification/RFC-BPR-0023.md"}},{"criterion":"Operator workflow","outcome":"pass","revision":"eb7d72b61","evidence":{"kind":"artifact","detail":"Private history, provenance qualification, composition, split, draft, publication, stale states and deletion are documented; docs/ux/private-review-workflow.md"}},{"criterion":"Developer boundaries","outcome":"pass","revision":"eb7d72b61","evidence":{"kind":"artifact","detail":"Core/host/Qt, store/protocol ports, async identity, schema, quotas, crash recovery and focused checks are documented; docs/architecture/system-architecture.md"}},{"criterion":"Conservative simplification","outcome":"pass","revision":"eb7d72b61","evidence":{"kind":"artifact","detail":"Qt composition dialog factory and parent palette flow simplified without behavior change; focused rebuild and panel row pass; src/adapters/qt/src/private_composition_editor.cpp"}},{"criterion":"Verification","outcome":"pass","revision":"eb7d72b61","evidence":{"kind":"artifact","detail":"Documentation gate, 64 documentation tests, scoped board validator, diff check, Qt build and 1/1 focused test pass; M9 render digests unchanged; docs/verification/RFC-BPR-0023.md"}}]
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
