---
schema: "implement-rfc.card.v1"
id: "BPR-0228"
title: "Compose review-ledger actionability into the shipped workbench"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0226","BPR-0238"]
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

## Implementation

- The composition root creates one ledger host only after accepting the exact
  host/account/repository/pull-request authority and retires it on matching
  invalidation.
- The selected-diff projection supplies exact file/material observations.
  Unknown material stays actionable; an accepted receipt projection re-runs
  the workbench actionability observation when its actionability changes,
  without replacing the accepted authority or detaching the selected diff.
- Seen, read, accepted, and excluded actions dispatch semantic receipt commands.
  Notes, reviewed heads, and defects use the same host authority.
- Store-open failure installs a typed unavailable store, disables the private
  actions, and leaves the public GitHub/epoch/diff path usable.

## Verification

`application_review_ledger_composition_contract` runs seed, reopen, and
store-obstruction processes through the shipped executable. Seed drives the
real drawer selection and seven receipt actions, then stores the three private
record kinds. The same seed process proves that filtering retains the selected
comparison and that changed material reopens the receipt and returns the file
to actionability. Reopen loads the receipt, carries it to the fixture's newer
head, and reloads the exact prior private records. The obstruction process
still opens and renders the selected diff while presenting private progress as
unavailable. Its source guard rejects direct fixture access to the concrete
store.
