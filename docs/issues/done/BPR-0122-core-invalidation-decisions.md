---
id: BPR-0122
title: Core invalidation decisions from bounded material observations
status: Done
rfc: RFC-BPR-0007
depends_on: [BPR-0121]
---

# Core invalidation decisions from bounded material observations

When an epoch advances, some receipts survive and some must reopen. The core
decides which, from a bounded observation the host constructs — never by
holding the ledger and never by reading storage.

"A new epoch preserves a receipt only when the compared material identity
remains equivalent" is the rule. Precision is the claim worth proving: a change
to one file must not invalidate a receipt on another, and an unchanged file
must survive an epoch advance that changed its neighbours.

## Required direct evidence

- A fixture advance with mixed changed and unchanged material invalidates
  exactly the changed receipts, asserted per file rather than by a count.
- A file whose content is identical under a new epoch keeps its receipt, and a
  file whose material identity changed loses it.
- The observation the core consumes is bounded: the test constructs one
  directly and the core never asks for more.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

`carry_over` decides one receipt's fate from one bounded
`file_material_observation` the host constructs. The core reads no epoch, no
repository, and no ledger to reach the decision, and the disposition is a named
value — `retained`, `reopened_material_changed`, or `file_absent` — rather than
a boolean, because "not retained" covers two different situations that a caller
must handle differently.

Precision is asserted per file rather than by counting invalidations. The
fixture holds an unchanged file, a file whose material moved, and a file absent
from the advance, and the unchanged receipt is checked to come back byte for
byte *after* its neighbour was reopened. A count would have passed even if the
wrong file were invalidated.

### Exclusion deliberately survives

Reopening a receipt clears the read progression and the acceptance and
re-attaches to the new material, but it preserves exclusion. That is a decision
worth stating rather than an oversight: excluding a file is a standing
statement about the file — a generated artefact stays uninteresting when it is
regenerated — and not a claim about particular bytes. Reopening it on every
advance would make the setting useless for exactly the files it exists for.

A test pins it, so a later change to the reopening rule cannot quietly drop the
distinction.

### An absent file is not a changed file

A receipt whose file is no longer compared is reported `file_absent` and left
untouched, rather than being retained. Retaining it would leave a receipt
attached to material that nothing compares. An observation naming a different
file takes the same path, so a mismatched observation can never be mistaken for
a retention.

### Non-vacuity

Two behaviours were broken separately, rebuilt, and observed to fail for their
own reason, then restored to green:

- Reopening clearing `Excluded` too — "exclusion survives a material change".
- The material comparison forced true so every receipt is retained — "a file
  whose material moved has its receipt reopened".
