---
schema: "implement-rfc.card.v1"
id: "BPR-1244"
title: "Windows: a new symbolic link in a Review Cycle is left out of the publication candidate and preview without a word"
status: "Ready"
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

## Cross-platform half: report (red-test backlog milestone, macOS arm64, 2026-09-30)

The report part of the scope is done on every platform; reading the link on
Windows is left to the Windows station.

Why the row was silent: the added link's final side is `unavailable`, which is
not a known state, so its improvement over the starting capture is `unknown`
and not a change. With no gap, `plan_outgoing` never listed the row.

Now `outgoing_side::Unread` keeps the manifest's reason for a path the capture
saw and did not read. A final side seen and not read gets the new gap
`final_unavailable`, unless neither capture read a path that P has, where
nothing is known to have changed and a sparse checkout would otherwise list
every path it leaves out. `outgoing_plan::Unread` counts these rows. The
preview says, after the candidate tree, "N path(s) the final capture could not
read ... not in the candidate as captured", and lists each under Not included
with its reason: for this card, "the final capture saw it but could not read
it (this platform cannot read its link target); it is not in the candidate".
The candidate still carries P's entry (none, for a new link) and stays
buildable; nothing is silently omitted.

Checks on macOS arm64 (dev preset): `core_review_cycle_outgoing` adds
`unread_final_paths` (an unreadable new link, a file grown past the capture
budget, and a path neither capture read); with the new branch disabled the
first case fails, as a control. `review_cycle_outgoing`, `review_cycle_candidate`
and the other review-cycle rows pass (48 rows with the menu rows).

Still for the Windows station:

- Read `IO_REPARSE_TAG_SYMLINK` data in `local_workspace_source.cpp`, or keep
  the limitation and assert the reported row in `review_cycle_candidate`'s link
  case. Either way that row's link assertion on Windows needs the station.
- Windows and Linux runs of `core_review_cycle_outgoing`.
