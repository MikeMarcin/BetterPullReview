---
provisional_id: PACK-BPR-0022-01
title: Import RFC-BPR-0022, re-audit the live tree, and bind the evidence plan
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: []
---

# Import RFC-BPR-0022, re-audit the live tree, and bind the evidence plan

## Objective

Import the pack on main, reconcile every identity and baseline assumption, and create the live issue/evidence structure before implementation.

## Required final state

- The immutable pack is retained under `docs/rfcs/assets/` with a `LIVE_IMPORT` record naming source SHA, live destinations, ID mappings, and exclusions.
- The RFC and issue plan are present at reconciled live paths; `docs/rfcs/README.md` records RFC-BPR-0022 without changing the canonical fourteen-RFC sequence.
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
