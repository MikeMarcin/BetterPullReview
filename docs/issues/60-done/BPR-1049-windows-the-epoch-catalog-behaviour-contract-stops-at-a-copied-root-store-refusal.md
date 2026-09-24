---
schema: "implement-rfc.card.v1"
id: "BPR-1049"
title: "Windows: sqlite_epoch_catalog_behavior_contract stops at a copied-root store refusal (BPR-0878 item 2), so its later checks never run on Windows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win dsp-004 Windows sweep (BPR-0878 item 2); BPR-0887's Windows selector on bpr-main-win","found_at":"2b2e5549","reproduced_at":["86b04901","669a5a58"],"related_cards":["BPR-0878","BPR-0887","BPR-0876","BPR-0853"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1049--BPR-1049--4624419c5298.json"}
acceptance: [{"criterion":"AC1 sqlite_epoch_catalog_behavior_contract passes on Windows, every check after line 1,792 included","outcome":"pass","revision":"a2127e27","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, serial: the row passes in 93.09 s alone and in 102.68 s inside BPR-0887's selector ctest --preset dev -j1 -R \"^(sqlite_|contained_catalog_probe$)\", which passes 20 of 20 at 8393e2ed plus the change (a later reflow of the comment only).","path":"docs/issues/60-done/BPR-1049-windows-the-epoch-catalog-behaviour-contract-stops-at-a-copied-root-store-refusal.md#fixed-in-the-test-the-whole-row-passes-on-windows-bpr-main-win-2026-09-23"}},{"criterion":"AC2 A negative control without the narrowing fails at the same line and by the same name","outcome":"pass","revision":"a2127e27","evidence":{"kind":"command","detail":"With the narrowing compiled out (#if 0, reverted), the row fails at epoch_catalog_behavior_tests.cpp:1792, 'FAIL: a copy of the previous file migrates to v6', as at 86b04901 and 669a5a58.","path":"docs/issues/60-done/BPR-1049-windows-the-epoch-catalog-behaviour-contract-stops-at-a-copied-root-store-refusal.md#fixed-in-the-test-the-whole-row-passes-on-windows-bpr-main-win-2026-09-23"}},{"criterion":"AC3 macOS and Linux are unchanged: the POSIX path of the test does not change","outcome":"pass","revision":"a2127e27","evidence":{"kind":"freeform","detail":"The change is inside #if defined(_WIN32); the POSIX path is byte for byte unchanged, and the row passes there as BPR-0887 records (macOS 18/18, Linux 19/19)."}}]
---

# BPR-1049: Windows: sqlite_epoch_catalog_behavior_contract stops at a copied-root store refusal (BPR-0878 item 2), so its later checks never run on Windows

## Defect

`sqlite_epoch_catalog_behavior_contract` has failed on Windows since the first
full Windows sweep at `2b2e5549`. It is item 2 of [[BPR-0878]]. It stops at:

```text
FAIL: a copy of the previous file migrates to v6
```

That is `tests/sqlite_contract/epoch_catalog_behavior_tests.cpp:1792`, in
`lossless_occurrence_migration_contract`, line 1,792 of 6,661. Every check
after it never runs on Windows. The row is `WINDOWS_UNCONTAINED`, so this is
not the container.

The test builds a version-3 or version-4 catalogue in one owner-private root,
copies the root with `std::filesystem::copy(..., recursive)`, and opens the
copy to prove the migration to v6 is lossless. On Windows every directory and
file that a copy creates inherits its parent's entries. The stores require
the explicit, protected owner-private descriptor, as
`make_storage_root_owner_private` writes it, so they refuse the copy before
any migration runs. On POSIX, `std::filesystem::copy` keeps each entry's mode,
so the copy is as private as the original and the row passes there.

It is the [[BPR-0853]] family, a copied store file, and the same shape that
[[BPR-0876]] fixed for the review-restoration course's `shutil.copytree`.

[[BPR-0887]] is Blocked on it. That card changed the epoch catalogue's
`progress_guard`, and its Windows evidence needs this row to reach its later
checks.

## Evidence (bpr-main-win)

- At `669a5a58`, BPR-0887's Windows selector
  (`ctest -j1 -R "^(sqlite_|contained_catalog_probe$)"`) passed 19 of 20. The
  twentieth was this row, at line 1,792, in 1.55 s
  (`build/logs/bpr-0887-1046-main-win/bpr0887.log`).
- The row failed the same way at `86b04901` on 2026-09-17, as BPR-0887
  records. The test file has not changed since 2026-09-13.

## Scope

- Make the test's copy produce what a copy of a private root has to be on
  Windows: each copied directory and file narrowed to the owner-private
  descriptor, as the test's own roots already are
  (`test_platform::make_owner_private`). Keep POSIX exactly as it is.
- Do not relax the stores. They refuse an inherited descriptor on purpose.
- Then run the whole row on Windows, so the checks after line 1,792 run
  there for the first time.

## Acceptance

- AC1 `sqlite_epoch_catalog_behavior_contract` passes on Windows, every check
  after line 1,792 included.
- AC2 A negative control without the narrowing fails at the same line and by
  the same name.
- AC3 macOS and Linux are unchanged: the POSIX path of the test does not
  change.

## Fixed in the test; the whole row passes on Windows (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug), serial. The logs
are station records under `build/logs/bpr-0887-main-win/` and are not
committed.

**The fix.** After its `std::filesystem::copy`, the test now narrows each
copied directory and file with `test_platform::make_owner_private`, the same
call that made the original root owner-private. That gives each copy the
explicit, protected descriptor the stores require. It is inside
`#if defined(_WIN32)`. The POSIX copy keeps each entry's mode and is
untouched. The stores are unchanged.

| Run | Result |
| --- | --- |
| Negative control: the narrowing compiled out (`#if 0`) | Fails at line 1,792, `FAIL: a copy of the previous file migrates to v6`, as at `86b04901` and `669a5a58` (AC2) |
| The fix | `sqlite_epoch_catalog_behavior_contract` passes in 93.09 s, every check after line 1,792 included (AC1) |

**[[BPR-0887]]'s Windows selector, 20 of 20.** At `8393e2ed` with this
change,
`ctest --preset dev -j1 --output-on-failure -R "^(sqlite_|contained_catalog_probe$)"`
passes all 20 rows. `sqlite_epoch_catalog_behavior_contract` takes 102.68 s,
alongside the other nineteen (`rows20.log`). The only later change to the
file is a comment reflowed to carry this card's tag.

**macOS and Linux (AC3).** The change is inside `#if defined(_WIN32)`. The
POSIX path of the test is byte for byte what it was, and the row passes there
as BPR-0887 records (macOS 18/18, Linux 19/19).

**The same shape elsewhere, not measured here.** Two Git contract suites also
copy a tree with `std::filesystem::copy`:
`epoch_maintenance_integration_tests.cpp:1033` and
`git_cache_contract_tests.cpp:2419`. Whether those copies meet the same
refusal on Windows was not measured in this card.
