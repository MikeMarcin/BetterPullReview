---
schema: "implement-rfc.card.v1"
id: "BPR-1239"
title: "Windows: Save to file fails as 'unavailable' for a read-only or held destination, with no reason and no wait for a brief hold"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station read of the portable-rounds Save to file path, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1239--BPR-1239--7e90dbb9f4bb.json","related_cards":["BPR-1159","BPR-1040","BPR-1104"]}
acceptance: [{"criterion":"On Windows, a Replace over a read-only destination fails with its specific reason; bytes and attribute unchanged; no temporary file remains","outcome":"pass","revision":"55ec1ee673905671c0c573d0197655a91c0e7699","evidence":{"kind":"command","path":"tests/workspace_contract/review_cycle_export_tests.cpp","detail":"windows_refusals_name_their_reason: confirmed Replace over a FILE_ATTRIBUTE_READONLY file returns cycle_export_failure::read_only ('The file there is read-only; nothing was replaced.'), bytes and READONLY attribute unchanged, no temporary file. Failing-before control: with the original local_export_destination.cpp the row fails at this check."}},{"criterion":"On Windows, a destination held without FILE_SHARE_DELETE and released within the bound is replaced; one held past the bound fails with its specific reason and nothing changes","outcome":"pass","revision":"55ec1ee673905671c0c573d0197655a91c0e7699","evidence":{"kind":"command","path":"src/adapters/workspace/src/local_export_destination.cpp","detail":"A held destination answers MoveFileExW with ERROR_ACCESS_DENIED (probed on the station); a DELETE-access probe that meets ERROR_SHARING_VIOLATION classifies it as held, retried within platform::retry_while_held (about 1.3 s). Released after 150 ms: replaced. Held past the bound: cycle_export_failure::in_use, bytes unchanged, no temporary file."}},{"criterion":"review_cycle_export still passes on every platform; architecture_boundaries and architecture_boundary_contract pass","outcome":"pass","revision":"55ec1ee673905671c0c573d0197655a91c0e7699","evidence":{"kind":"command","path":"src/platform/include/bpr/platform/windows_sharing_retry.hpp","detail":"Windows windows-dev Debug: review_cycle_export, review_cycle_bundle, review_cycle_publish_desk, review_cycle_workbench, application_review_cycle_desktop_public_path, git_cache_contract, git_process_contract, architecture_boundaries and architecture_boundary_contract pass; clang-format clean. The POSIX move path is unchanged and the new enum values only add words; macOS and Linux runs of review_cycle_export are owed."}}]
---

# BPR-1239: Windows: Save to file fails as 'unavailable' for a read-only or held destination, with no reason and no wait for a brief hold

Found by bpr-main-win on 2026-09-29, reading the Windows branch of
`src/adapters/workspace/src/local_export_destination.cpp` at `bfc02529`.
Its rows (`review_cycle_export`) pass on Windows. The path is sound. This card
is about what the reviewer is told, and about one transient case.

## Observation

`move_into_place` maps every `MoveFileExW` failure other than
`ERROR_ALREADY_EXISTS` or `ERROR_FILE_EXISTS` to
`cycle_export_failure::unavailable`. Two ordinary Windows cases take that path,
and POSIX `rename` has neither:

- **A read-only destination** (`FILE_ATTRIBUTE_READONLY`). `MOVEFILE_REPLACE_EXISTING`
  fails with `ERROR_ACCESS_DENIED`. A confirmed Replace therefore fails, and
  nothing says the file is read-only.
- **A destination another process holds open without `FILE_SHARE_DELETE`.**
  This includes an editor that keeps the file open, and Defender's brief scan
  of a file written a moment ago. That hold is the shape [[BPR-1040]] measured:
  `ERROR_SHARING_VIOLATION`, and success about 50 ms later. A Replace right
  after an earlier save can fail for this reason alone.

Both fail safely: the destination is unchanged, and the temporary file is
removed. No row exercises either case.

## Scope

- Give the reviewer a specific reason for each case, in the application's
  words, distinct from a generic `unavailable`. Whether that is a new
  `cycle_export_failure` value or a detail on the existing one is the
  implementer's call. The core decides the words.
- Wait out a brief hold with a bounded retry on `ERROR_SHARING_VIOLATION` and
  `ERROR_LOCK_VIOLATION` only, and then report it. The git adapter has
  `detail::retry_while_held` (`src/adapters/git/src/windows_sharing_retry.hpp`,
  [[BPR-1040]], [[BPR-1104]]). The workspace adapter must not include another
  adapter. If it is shared, it moves to `src/platform/`, which exists for
  exactly this: one Windows definition that several adapters share. It would
  need the same header-only and no-policy limits, and `architecture_boundaries`
  must stay green.
- Never clear a read-only attribute or take a handle from another process to
  make the save succeed.

## Acceptance

- On Windows, a Replace over a read-only destination fails with its specific
  reason. The destination's bytes and attribute are unchanged, and no
  temporary file remains.
- On Windows, a destination held open by a test handle without
  `FILE_SHARE_DELETE`, and released within the bound, is replaced. One held
  past the bound fails with its specific reason, and nothing is changed.
- `review_cycle_export` still passes on every platform. `architecture_boundaries`
  and `architecture_boundary_contract` pass.

## Done on Windows (bpr-main-win, 2026-10-06)

**What Windows answers.** A probe on this station showed that
`MoveFileExW(…, MOVEFILE_REPLACE_EXISTING)` over a destination another
handle holds without `FILE_SHARE_DELETE` fails with `ERROR_ACCESS_DENIED`,
not a sharing violation. That is the same code a read-only file or a refused
permission gives. Once the handle closes, the same move succeeds. The first
version of this change waited only on sharing and lock violations, so it
reported a held destination as unavailable. The course caught that at "a
destination released within the wait is replaced".

**The change.**

- `application::cycle_export_failure` gains `read_only` and `in_use`, with
  stable names and their own words: "The file there is read-only; nothing
  was replaced." and "Another program has the file there open; nothing was
  replaced. Close it and save again." The CLI keeps them under
  `destination_unavailable`, with these words.
- `local_export_destination`'s Windows move tells access denied apart, in
  this order:
  1. a destination marked read-only fails at once as `read_only`, and its
     attribute is never cleared;
  2. otherwise it opens the destination for `DELETE` with full sharing as a
     probe. A sharing violation there means another handle holds the file:
     the move is repeated within the bound and, still held, reported
     `in_use`;
  3. anything else stays `unavailable`.

  Sharing and lock violations from the move itself take the same bounded
  wait. No handle of another process is taken.
- The bound is the Git adapter's `retry_while_held` ([[BPR-1040]],
  [[BPR-1104]]): eight waits from 5 ms, doubling, about 1.3 s. It moved to
  `src/platform/include/bpr/platform/windows_sharing_retry.hpp`, so the
  workspace adapter shares it without an adapter-to-adapter edge. It is
  header-only, includes no Qt, and is listed in the platform target. The
  caller now names which errors count as held: the Git adapter keeps
  sharing, lock and access denied, as before; Save to file names sharing and
  lock. The export target links `BetterPullReview::Platform` privately.

**Evidence** (`build/windows-dev` Debug, MSVC 14.51, whole tree, no
`warning C` line). `review_cycle_export_tests.cpp` gains a Windows section,
`windows_refusals_name_their_reason`:

- a confirmed Replace over a read-only file fails `read_only`, with its bytes
  and attribute unchanged;
- a destination held by a test handle without `FILE_SHARE_DELETE`, released
  after 150 ms, is replaced;
- one held past the bound fails `in_use`, unchanged;
- no temporary file remains.

Results:

- `review_cycle_export` passes. With the original
  `local_export_destination.cpp` it fails at its first check, "a read-only
  destination is refused as read-only and left as it was".
- `review_cycle_bundle`, `review_cycle_publish_desk`,
  `review_cycle_workbench`, `application_review_cycle_desktop_public_path`,
  `git_cache_contract` and `git_process_contract` (the moved retry's three
  Git call sites), `architecture_boundaries` and
  `architecture_boundary_contract` pass.
- clang-format 22.1.3 is clean on the changed files.

macOS and Linux build the new enum values and words. Their move path is
unchanged.

## macOS run (coordinator, 2026-10-06)

At 4cf46a609 on macOS arm64 Debug (build/dev in a clean worktree): 12/12 of the
canary (contained launch courses and the durable-course row), agent parent death,
cycle export/bundle/publish-desk/workbench, git cache and process contracts,
architecture boundaries and the network isolation manifest passed.

**Note: Linux run at `4cf46a609` (station `bpr-main-linux`, WSL2, GCC 15.2.0,
`build/dev` rebuilt whole, `-j1`).** The build had no GCC warning or
error; the only "Warning" lines are CMake configure notices in Qt's own CMake
files. 12 of 12 passed: `qt_agent_launch_canary` (63.1 s),
`qt_agent_launch_canary_durable_course`, `agent_parent_death` (20.8 s),
`review_cycle_export`, `review_cycle_bundle`, `review_cycle_publish_desk`,
`review_cycle_workbench` (34.7 s), `git_cache_contract`,
`git_process_contract`, `architecture_boundaries`,
`architecture_boundary_contract` (188.2 s) and
`test_network_isolation_manifest`. Nothing needed a fix. Done is not
reopened.
