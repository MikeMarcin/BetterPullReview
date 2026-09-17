---
schema: "implement-rfc.card.v1"
id: "BPR-0851"
title: "A fetched pack with a missing object leaves the Git cache repair_required"
status: "Backlog"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-004 BPR-0846 worker","found_at":"a5168c6d","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0851: A fetched pack with a missing object leaves the Git cache repair_required

## Defect

The BPR-0846 course `git_haves_trimmed_pack_missing-object` serves a pack
that references an object the cache does not hold. `index-pack --strict`
exits 128 (`did not receive expected object`), and the import is refused, as
it should be. The cache then ends in `repair_required` (code 13), with or
without BPR-0846's fix (`build/r2-logs/dsp4-0846-negative-control.log`,
`MISSING` lines).

The likely cause, not yet proven, is that `index-pack` leaves a `tmp_pack_*`
file in the object store when it dies. The rollback check then finds an
object store that is not the expected pre-import inventory. If so, a server
that sends a broken pack can push the cache into repair instead of a plain
refusal.

## Scope

- Confirm the cause from the object store after the refusal.
- Make a refused import that leaves only `index-pack`'s own temporary files
  roll back to a usable cache. Keep recognising and refusing any other
  unexpected store change. This is the Git cache durability contract, so give
  it a critical review.

## Acceptance

- After a missing-object refusal, the cache is usable: the earlier head stays
  latest, and a later valid import succeeds without repair. A course shows it.
- An unexpected extra pack in the store is still reported as needing repair.
