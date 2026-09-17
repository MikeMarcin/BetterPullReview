---
schema: "implement-rfc.card.v1"
id: "BPR-0851"
title: "A fetched pack with a missing object leaves the Git cache repair_required"
status: "Ready"
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

## Cause (dsp-004 scout, confirmed by experiment)

With `/usr/bin/git` 2.50.1 and the product's flags, `index-pack` dies with 128
and leaves `objects/pack/tmp_pack_XXXXXX` in two cases:

- a missing object (`did not receive expected object`);
- a truncated pack (`early EOF`).

No `tmp_idx_*` is left.

The "only the expected pack was added" check is never reached. The failure
path in `prepare_revision` (`installed_git_cache.cpp` ~3436-3450) calls
`validated_inventory`, whose scan (~1483-1510) refuses any `objects/pack`
entry other than `pack-<hex>.pack` or `.idx` as `invalid_repository`. The
refusal therefore returns `repair_required`.

This is not a regression. `git_cache_contract_tests.cpp` ~1346-1352 already
works around "failed-index residue" by resetting the cache. A server can
trigger it with a pack whose checksum is valid but whose content is not.
A timeout or cancel probably leaves the same residue; that is unverified.

## Scope

- Confirm the cause from the object store after the refusal.
- Make a refused import that leaves only `index-pack`'s own temporary files
  roll back to a usable cache. Keep recognising and refusing any other
  unexpected store change. This is the Git cache durability contract, so give
  it a critical review.

- Suggested shape: before `index-pack`, snapshot the `objects/pack` names,
  under the cache lock. On the failure path only, remove regular, private,
  non-symlink files that:
  - match `tmp_pack_*` or `tmp_idx_*`;
  - were absent from the snapshot;
  - are owned by this uid;
  - have a link count of 1.

  Remove each one exactly, then sync the directory and run the existing
  inventory and rollback flow. Any other new or pre-existing entry still
  gives `repair_required`, and nothing that was planted is deleted.
- Drop the `opened->reset()` workaround in course 5 of
  `git_cache_contract_tests.cpp` (~1346) and assert that the cache is usable.
- From BPR-0846's critical review, in the same contract area:
  - make `git_haves_trimmed_pack_missing-object` assert the product's failure
    code, not only its outcome;
  - add a process-contract row: an accepted `index-pack` exit 1 with
    non-empty stderr is `malformed_output`;
  - move the new `index_pack` doc comment in `git_process_port.hpp` above
    the port's group note.

## Acceptance

- After a missing-object refusal, the cache is usable: the earlier head stays
  latest, and a later valid import succeeds without repair. A course shows it.
- An unexpected extra pack in the store is still reported as needing repair.
  So is a pre-existing `tmp_pack_*`, and the planted file is not deleted.
- The missing-object course asserts the failure code, and the process
  contract covers exit 1 with stderr.
