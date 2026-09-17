# dsp-004 Windows confirmation: BPR-0846's three courses

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-0846`. The base is
`e6b7193e19862ba3f0d27c3096ba30ddc0ea60cf`, which is main and contains
[[BPR-0846]]'s candidate `bbdf8cc0`. The card is main-owned; this file is
evidence only. No product code, card, RFC or manifest was changed.

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community; Qt 6.10.3
`msvc2022_64`; Git for Windows `2.55.0.windows.3`. The shell token is not
elevated.

**Build.** `cmake --preset windows-dev` and a full `cmake --build` of
`build/windows-dev` (Debug) succeeded.

## The three courses, 3 runs each through ctest

`ctest --preset windows-dev -R "^git_haves_trimmed_pack_(update|missing-object|unconnected)$" --output-on-failure -V`,
run three times. Every run passed, and the markers were identical in all
three runs.

| Course | Times (s) | Markers |
| --- | --- | --- |
| `git_haves_trimmed_pack_update` | 9.22, 6.69, 8.03 | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6`, `haves-update-loads:pass` |
| `git_haves_trimmed_pack_missing-object` | 4.39, 4.44, 4.62 | `EPOCH_EFFECT_FAILED:revision-prepare:5`, `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, final cache failure `EPOCH_CACHE_FAILURE:13:`, `haves-missing-object-refused:pass` |
| `git_haves_trimmed_pack_unconnected` | 3.45, 3.43, 3.67 | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, `UNCONNECTED_CACHE_FAILURE:10:`, `CONNECTED_CACHE_FAILURE:-1`, `haves-unconnected-residency-refused:pass` |

The missing-object course's final cache failure is **13**, the same as macOS
([[BPR-0851]]).

## The gate and the sibling

- `test_network_isolation_manifest`: **passed**, 6.41 s,
  `TEST_NETWORK_ISOLATION_OK:configured=360:registrations=346:backend=windows-appcontainer`.
- `epoch_maintenance_integration`: **failed**, 3.13 s, `FAIL: construct real
  SQLite catalog`. This is a new defect, reported uncarded and not fixed here.
  Measured with temporary diagnostics that were then reverted:
  - The failure is in `verify_real_migrated_retained_refs`. The first fixture
    (`migration`) builds its catalog. The `migrated` fixture then reopens the
    catalog that `std::filesystem::copy` copied into a second temporary root.
    `make_epoch_catalog` refuses that copy with
    `epoch_catalog_failure::invalid_permissions` (2).
  - The cause is not `aa3c6ef1` ([[BPR-0833]]'s Windows root creation). With
    `catalog_database.cpp` taken from `aa3c6ef1^`, the same course fails the
    same way.
  - The migration course was added on 2026-09-12 (`72d40b4f`, `776bc0d4`,
    `90d4ba28`). The last recorded Windows pass of this test on the station is
    from 2026-08-27, before that course existed. It may never have passed on
    Windows.
  - A likely reading, not proven: the recursive copy gives the copied
    `state/v1` objects inherited descriptors, not the explicit private ones,
    and the adapter refuses a wider object rather than repairing it
    ([[BPR-0806]]).

## Containment

The three rows are pinned `WINDOWS_UNCONTAINED` with reason `dos-volume-name`.
Each course was run inside the AppContainer directly:
`build/windows-dev/Debug/better_pull_review_network_sandbox.exe <test exe> --<course>`.
This is the same way [[BPR-0833]] and [[BPR-0845]] were measured. A temporary
`std::set_terminate` hook printed the escaping exception. Every trial edit was
reverted and the target rebuilt.

**None of the three passes contained.** They all refuse in the same two
places:

1. **First refusal: `dos-volume-name`, as pinned.** All three exit 3 in the
   shared `temporary_root` constructor, at
   `Path = std::filesystem::canonical(Path)`
   (`haves_trimmed_pack_integration_tests.cpp:210`):

   ```text
   TRIAL-TERMINATE canonical: Access is denied.: "C:\Users\mikem\AppData\Local\Packages\bprautomatedtest\AC\Temp\bpr-haves-update-196465363520500"
   ```

   (`bpr-haves-missing-…` and `bpr-haves-unconnected-…` are the same.) The
   copied reason was not measured before, but it matches this measurement.

2. **Next refusal: `qprocess-named-pipe`.** A deeper trial replaced both
   `canonical` calls (line 68 for the Git executable, line 210) with
   `lexically_normal`. All three courses then exit 1 at `FAIL: start fixture
   Git` (line 114). The fixture's `QProcess` fails to start:

   ```text
   TRIAL-DEEPER-QPROCESS pipe: The system cannot find the file specified.
   ```

   The courses therefore depend on a second uncontainable cause behind the
   first. Nothing past the fixture Git start was measured.

The manifest was not changed on the lane.

## BPR-0851 and BPR-0852 on Windows (task-20260917-win-0653-resume)

The base is `a1864fa296ce3450ce0e0470f36bd0d95cd59470`, which contains the
candidate `15eb1540`. This is evidence only; the cards are not edited. There
is a full `windows-dev` Debug build, and Git for Windows is
`2.55.0.windows.3`.

**One run each through ctest: all six pass.**

| Course | Time (s) | Result |
| --- | --- | --- |
| `git_process_contract` | 39.78 | `Git process contract passed` |
| `git_cache_contract` | 69.14 | `Git cache contract passed` |
| `git_sparse_cache_contract` | 27.35 | `Git sparse cache contract passed` |
| `git_haves_trimmed_pack_update` | 7.14 | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6`, `haves-update-loads:pass` |
| `git_haves_trimmed_pack_missing-object` | 11.04 | `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, `EPOCH_CACHE_FAILURE:10:fatal: did not receive expected object cc214650eaea68363a619361da152c233956845a`, `EPOCH_EFFECT_FAILED:revision-prepare:6`, **`MISSING_REFUSAL_REASONS:27`** (no 26), `haves-missing-object-refused:pass` |
| `git_haves_trimmed_pack_unconnected` | 4.73 | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, **`UNCONNECTED_CACHE_FAILURE:10:fsck reported 1 missing commit, 1 missing blob, 2 broken links`**, `CONNECTED_CACHE_FAILURE:-1`, `haves-unconnected-residency-refused:pass` |

Both key markers match macOS exactly. Before the candidate, the
missing-object course ended with `EPOCH_CACHE_FAILURE:13:` and
`EPOCH_EFFECT_FAILED:revision-prepare:5` (see above).

**Git for Windows' `tmp_pack_*` against the cleanup.** A temporary trace
around the candidate's removal loop in `remove_failed_index_residue`, reverted
and rebuilt, ran on the missing-object course directly:

```text
TRIAL-0851 candidates=1
TRIAL-0851 candidate=tmp_pack_rFH5sQ attributes=0x21 readonly=1 identity=1 private_single_link=1 removed=1 missing_after=1
```

- Git for Windows leaves one `objects/pack/tmp_pack_XXXXXX`, which was not
  there before the index. Its attributes are `0x21`: `FILE_ATTRIBUTE_READONLY`
  and `FILE_ATTRIBUTE_ARCHIVE`.
- It passes the cleanup's checks. It has an exact identity, and
  `private_regular_file` returns true. On Windows that function checks for a
  regular file with no reparse point and `nNumberOfLinks == 1`; it has no
  owner or descriptor check.
- **The removal succeeds even though the file is read-only.**
  `remove_exact_private_file` renames it to its quarantine name and
  `std::filesystem::remove` deletes it. The name is then missing, and the
  course's `index_residue` count is 0.
- In `git_cache_contract`, the hard-link plant (`tmp_pack_BPRln1` /
  `tmp_idx_BPRln2`) is kept on Windows and passes. The shared-mode plant
  (`tmp_idx_BPRshr`) is POSIX-only by the contract's own `#if`, because
  Windows' private check has no mode to widen.
