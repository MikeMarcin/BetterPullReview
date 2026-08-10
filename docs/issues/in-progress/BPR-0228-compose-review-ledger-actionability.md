---
id: BPR-0228
title: Compose review-ledger actionability into the shipped workbench
status: In Progress
rfc: RFC-BPR-0007
depends_on: [BPR-0226, BPR-0238]
---

# Compose review-ledger actionability into the shipped workbench

The application opens a review-ledger store and fixture helpers read and write
it directly, but it does not construct `review_ledger_application_host` and
the workbench observation adapter reports every file as actionable.

## Required final state

- The real application composes the ledger host over the store it opens.
- Read, accepted, excluded, and seen gestures travel through core intent,
  persistence request, typed completion, and changed projection.
- Notes, last-reviewed heads, and defect metadata from BPR-0238 enter through
  the same bounded host authority rather than a second direct-store path.
- Workbench file/actionable observations derive from accepted ledger facts
  rather than treating every file as actionable.
- A two-process public trace proves persistence without direct store access
  from fixture helpers.
- Store failure leaves shared GitHub review content usable and states that
  private progress is unavailable.
