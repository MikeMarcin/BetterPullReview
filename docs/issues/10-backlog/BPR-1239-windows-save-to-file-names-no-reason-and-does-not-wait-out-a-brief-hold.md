---
schema: "implement-rfc.card.v1"
id: "BPR-1239"
title: "Windows: Save to file fails as 'unavailable' for a read-only or held destination, with no reason and no wait for a brief hold"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station read of the portable-rounds Save to file path, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1239--BPR-1239--7e90dbb9f4bb.json","related_cards":["BPR-1159","BPR-1040","BPR-1104"]}
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
