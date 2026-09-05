---
id: BPR-0576
title: Import RFC-BPR-0023, re-audit the live tree, and bind the evidence plan
status: Backlog
rfc: RFC-BPR-0023
milestone: Post-Pack v2
depends_on: []
---

# Import RFC-BPR-0023, re-audit the live tree, and bind the evidence plan

## Objective

Import the pack on main, reconcile every identity and baseline assumption, and create the live issue/evidence structure before implementation.

## Required final state

- The immutable pack is retained under `docs/rfcs/assets/` with a `LIVE_IMPORT` record naming source SHA, live destinations, ID mappings, and exclusions.
- The RFC and issue plan are present at reconciled live paths; `docs/rfcs/README.md` records RFC-BPR-0023 without changing the canonical fourteen-RFC sequence.
- All provisional cards have collision-free live `BPR-NNNN` IDs allocated from `NEXT_ID`; filenames, front matter, cross-references, and dependencies agree.
- A live source audit confirms or updates every item in `supporting/CURRENT_STATE_SOURCE_AUDIT.md`, including schema, draft persistence, agent metadata, side chat, and workbench composition.
- The architecture allocation, acceptance claim matrix, documentation ledger, schema/migration matrix, protocol capability matrix, and designated-mutation evidence plan exist on live cards.
- Structure and documentation gates pass on the import commit.

## Non-goals

- Implementing feature behavior in the import commit
- Assigning live IDs by hand without the repository allocator
- Copying this implementation-order file as a second mutable tracker

## Completion evidence

- Import commit SHA
- LIVE_IMPORT mapping
- Allocated-card inventory and dependency graph
- Structure/documentation gate output
- Live-audit diff against pack baseline

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

The pack retention, identity allocation, RFC/index import, and initial source
reconciliation are complete in the import slice. Detailed implementation-readiness
planning and evidence selection remain open; this card is not claimed Done.
