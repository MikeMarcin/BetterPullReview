---
id: BPR-0170
title: Persist enrollment across restarts
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0162]
---

# Persist enrollment across restarts

BPR-0162 built the retention *policy* — what is kept, what departs, what the
queue shows — and no durable store. The register lives for as long as the
process does.

That is a real gap, not a technicality. Observer enrollment is supposed to
persist "until closure or manual removal", and a queue rebuilt from scratch on
every launch would re-derive membership from whatever the paths look like
*now*, which is precisely the recomputation BPR-0162 exists to avoid. The
policy would be correct and the behaviour would be wrong.

## What is needed

A store beside the review ledger, following the pattern RFC-BPR-0007
established: its own SQLite database under `state/v1`, a `PRAGMA user_version`,
forward-only migration, and a typed failure enumeration. The state directory
comes from `detail::open_state_directory`, which BPR-0132 made the one
bootstrap.

## Required direct evidence

- An item enrolled by a path rule, with the path then removed from the pull
  request, is still enrolled after a separate process opens the store — a
  restart claim needs a second process, not a reopened handle.
- A departed item keeps its departure reason across the restart, so the
  reviewer is told what happened rather than seeing it silently absent.
- The store holds no source and no comment body.
- Deleting the enrollment store loses only enrollment, exactly as deleting the
  review ledger loses only receipts.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
