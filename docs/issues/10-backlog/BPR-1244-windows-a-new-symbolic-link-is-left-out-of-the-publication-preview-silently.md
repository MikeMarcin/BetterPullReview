---
schema: "implement-rfc.card.v1"
id: "BPR-1244"
title: "Windows: a new symbolic link in a Review Cycle is left out of the publication candidate and preview without a word"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run of review_cycle_candidate, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1244--BPR-1244--48f9a10743e0.json","related_cards":["BPR-1150","BPR-1173"]}
---

# BPR-1244: Windows: a new symbolic link in a Review Cycle is left out of the publication candidate and preview without a word

Found by bpr-main-win on 2026-09-30: Windows 11 Home 10.0.26200, MSVC 19.51,
`build/windows-dev` Debug, at `e9e65eab`.

## Observation

`review_cycle_candidate` fails on Windows at `a link is its target bytes, never
followed`. The machine can create symbolic links, so `try_symlink` succeeds and
the course adds `docs-link -> README.md` between R0 and Rn.

A probe, not committed, showed the following:

- The built candidate has no `docs-link` entry at all.
- The publication preview does not name it, either under "Outgoing (P ->
  candidate)" or under "Not included". The reviewer is not told that a path
  they added was left out.

The cause is in the Windows branch of
`src/adapters/workspace/src/local_workspace_source.cpp`. Its `read_link` returns
`workspace_failure::unsupported` for every symbolic link ("Reparse-data parsing
is not implemented; the entry stays visible"). So the capture records the link
as unavailable, and `review_cycle_outgoing.cpp`'s `side` then gives it
`outgoing_state::unavailable`. Nothing in the preview reports a selected path
in that state.

On macOS and Linux the link is read with `readlinkat`. The row passes there.

## Scope

- **Report, whatever else is done.** A selected path whose outgoing side is
  `unavailable` appears in the preview with its reason, and the plan's counts
  include it. The candidate is never presented as complete while it silently
  omits such a path. This holds on every platform.
- **Then read the link on Windows.** Parse `IO_REPARSE_TAG_SYMLINK` reparse
  data (`FSCTL_GET_REPARSE_POINT`) for the stored target text. It must be
  bounded by `maximum_link_bytes`, never follow the link, and refuse an
  absolute or device target in the same way as POSIX. Alternatively, keep the
  limitation, and record it in the preview and in the RFC's limits.

## Acceptance

- On Windows, with a symbolic link added in a round, the preview names the
  path. Either the candidate contains it as mode `120000` with its target bytes,
  or the preview says it was not built and why.
- `review_cycle_candidate` passes on Windows, with the link case asserting
  whichever behaviour was chosen.
- macOS and Linux still pass.
