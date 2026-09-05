---
provisional_id: PACK-BPR-0022-15
title: Update product/architecture/operator documentation and simplify the implementation
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-13, PACK-BPR-0022-14]
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
