---
id: BPR-0122
title: Core invalidation decisions from bounded material observations
status: Backlog
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
