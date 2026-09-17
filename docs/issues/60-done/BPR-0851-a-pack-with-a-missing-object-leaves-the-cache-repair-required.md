---
schema: "implement-rfc.card.v1"
id: "BPR-0851"
title: "A fetched pack with a missing object leaves the Git cache repair_required"
status: "Done"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-004 BPR-0846 worker","found_at":"a5168c6d","run":"run-20260916-stations","dispatch":"dsp-004"}
acceptance: [{"criterion":"AC1 after a missing-object refusal the cache is usable: the earlier head stays latest and a later valid import succeeds without repair, and a course shows it","outcome":"pass","revision":"791987d9","evidence":{"kind":"command","detail":"git_haves_trimmed_pack_missing-object requires the process_failure reason (27, no 26), no tmp_* residue, Base latest, and after a host restart a normal import of Head2 without repair. macOS 3/3 at 15eb1540 and at the integrated head (build/r2-logs/dsp4-0851-courses-x3.log, dsp4-int1-ctest.log); Linux Git 2.53.0 and Windows Git for Windows 2.55.0 pass with 27 (docs/verification/dsp4-{linux,windows}-confirmation.md). Negative control without the cleanup: 13 (dsp4-0851-negative-control.log). Course 5 of git_cache_contract drops its reset workaround and imports sparsely after the refusal."}},{"criterion":"AC2 an unexpected extra pack, or a pre-existing tmp_pack_*, is still reported as needing repair and the planted file is not deleted","outcome":"pass","revision":"791987d9","evidence":{"kind":"command","detail":"git_cache_contract courses 6-8 (fault injection only on the contract target) plant a pre-existing tmp_pack_*, an extra pack, and hard-linked or non-private tmp files: each gives repair_required and every planted file remains; since 791987d9 each course inspects only its own store and passes alone (dsp4-0851-fix2-trial-after.log); both contracts pass (dsp4-0851-fix2-contracts.log); Linux and Windows contracts pass. Critical review: no blocking finding."}},{"criterion":"AC3 the missing-object course asserts the failure code, and the process contract covers exit 1 with stderr","outcome":"pass","revision":"15eb1540d302795961f9f913af9942ff40a4bf2f","evidence":{"kind":"command","detail":"The missing-object course asserts reason 27 and no 26; git_process_contract's fake index-pack BPR_EXIT_ONE_ERROR row pins accepted exit 1 with stderr as malformed_output (dsp4-0851-verification.log); both pass on all three platforms."}}]
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

## Fix (dsp-004, Mac)

**What changed.** `prepare_revision_impl` lists the names in `objects/pack`
just before `index-pack` runs, under the operation mutex and the cache lock.
The list is capped at 4 x the pack-count limit + 16.

If `index-pack` fails, and the failure is not `reap_failure`, the cache
removes only the entries that meet all of these:

- the name is `tmp_pack_` or `tmp_idx_` followed by exactly six ASCII letters
  or digits, which is Git's mkstemp template;
- the name was absent from the list;
- the file is regular, not a symlink, private, owned by this uid and has one
  link.

`remove_exact_private_file` removes each file by exact identity, and the
directory is synced once. The existing inventory and rollback checks then run
unchanged, so any other change to the store still gives `repair_required`, and
nothing planted is deleted. The sparse import shares this code. A timed-out or
cancelled `index-pack` takes the same branch, but no test covers that.

**Tests.**

- `git_haves_trimmed_pack_missing-object` requires the `process_failure`
  reason with no `repair_required` reason, and finds no `tmp_*` file. Base
  stays the latest head. After a host restart, a normal import of Head2 loads
  without repair.
- Course 5 in `git_cache_contract_tests.cpp` no longer resets the cache. It
  requires `process_failure` with no residue, then imports the same pack
  sparsely.
- Courses 6-8 use test-only fault injection to plant a pre-existing
  `tmp_pack_*`, an extra pack, and temporary files that are hard-linked or,
  on POSIX, not private. Each still gives `repair_required`, and every
  planted file remains.
- A process-contract row pins that an accepted `index-pack` exit 1 with stderr
  is `malformed_output`.
- The `index_pack` doc comment is separated from the port's group note by a
  blank line.

**Runs (macOS, `build/r2-dev`).**

- The git process, cache and sparse-cache contracts,
  `epoch_maintenance_integration`, both kill-recovery courses,
  `application_epoch_maintenance_public_path`, `architecture_boundaries` and
  `test_network_isolation_manifest` pass (`dsp4-0851-verification.log`).
- The three haves courses pass 3/3 (`dsp4-0851-courses-x3.log`).
- Negative control: without the cleanup, the missing-object course reports
  `EPOCH_CACHE_FAILURE:13`, and sparse course 5 fails
  (`dsp4-0851-negative-control.log`).

**Before this fix.** `bpr-linux` (Git 2.53.0) and `bpr-win` (Git for Windows
2.55.0) both reproduced code 13 on the missing-object course
(`docs/verification/dsp4-linux-confirmation.md`,
`docs/verification/dsp4-windows-confirmation.md`).

**Still open.** A critical review, and the Linux and Windows runs of the fix.
Git for Windows may create `tmp_pack_*` read-only, or with a descriptor that
the private check refuses; in that case the cleanup fails closed at 13.

## Critical review and correction (dsp-004)

An `rfc-reviewer-critical` found **no blocking issue**.

- **Ownership proof.** A hard link, symlink or reparse swap fails the identity
  check. A swap after the quarantine rename deletes nothing.
- **Failing closed.** A listing error or the bound removes nothing. The
  inventory still refuses a `.rev` file, an unknown name, a pre-existing
  `tmp_*` and a crash-left `.bpr-file-cleanup-*`. Fault injection is defined
  only on the cache-contract test target.
- **Timeout and cancel.** On those paths the child is killed and reaped
  before the cleanup runs, and `reap_failure` is excluded.
- **BPR-0852.** Unrecognised lines are only counted, the stderr limit holds,
  and output over 1024 bytes carries no text. The success rule is unchanged.

The reviewer's findings, with their dispositions:

1. Courses 7 and 8 matched the planted files of earlier courses: run alone,
   they failed. **Corrected.** Each course now inspects only its own cache's
   object store. Run alone, courses 6, 7 and 8 each pass, and so do both
   contracts (`build/r2-logs/dsp4-0851-fix2-trial-{before,after}.log`,
   `dsp4-0851-fix2-contracts.log`).
2. Windows was unverified. **Since run.** On `bpr-win`, Git for Windows
   leaves `tmp_pack_*` with attributes READONLY|ARCHIVE. The file passes the
   single-link and private checks, and its removal succeeds.
3. A same-uid process writing a `tmp_pack_*` file into the locked store during
   `index-pack` now has that file removed. **Recorded:** the cache lock is the
   authority boundary.
4. The agent workspace path carries cache detail into an agent invocation's
   stderr. **Recorded:** the new summary is fixed words and counts, and the
   existing `index-pack` stderr already named an object id there.

**Station runs of `15eb1540`.**

- **Linux (Git 2.53.0).** All 7 rows pass, with `MISSING_REFUSAL_REASONS:27`
  and the same summary as macOS. An `LD_PRELOAD` observer saw the product
  quarantine and remove exactly `tmp_pack_vijD1a` (mode 400, 1 link, own uid)
  (`docs/verification/dsp4-linux-confirmation.md`).
- **Windows (Git for Windows 2.55.0).** The process, cache and sparse-cache
  contracts and the three haves courses pass, with
  `MISSING_REFUSAL_REASONS:27` and no 26
  (`docs/verification/dsp4-windows-confirmation.md`).
