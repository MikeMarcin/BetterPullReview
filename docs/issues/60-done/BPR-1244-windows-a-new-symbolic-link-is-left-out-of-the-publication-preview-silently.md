---
schema: "implement-rfc.card.v1"
id: "BPR-1244"
title: "Windows: a new symbolic link in a Review Cycle is left out of the publication candidate and preview without a word"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run of review_cycle_candidate, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1244--BPR-1244--48f9a10743e0.json","related_cards":["BPR-1150","BPR-1173"]}
acceptance: [{"criterion":"On Windows, with a symbolic link added in a round, the preview names the path; either the candidate contains it as mode 120000 with its target bytes, or the preview says it was not built and why","outcome":"pass","revision":"3ee80cb3","evidence":{"kind":"command","detail":"Windows read_link now reads the relative symbolic link's stored target (FSCTL_GET_REPARSE_POINT, not followed); review_cycle_candidate shows Git reading docs-link back from the candidate as mode 120000 with blob README.md. Paths that stay unreadable are reported by the Mac half's final_unavailable row."}},{"criterion":"review_cycle_candidate passes on Windows, with the link case asserting whichever behaviour was chosen","outcome":"pass","revision":"3ee80cb3","evidence":{"kind":"command","detail":"review_cycle_candidate passes on Windows (12.79 s) with its link case; it failed at 'a link is its target bytes' before (negative control); workspace_source, review_cycle_capture/outgoing/publication/publisher, core_review_cycle_outgoing and architecture_boundaries pass."}},{"criterion":"macOS and Linux still pass","outcome":"pass","revision":"e464f13da","evidence":{"kind":"command","detail":"The Windows change is Windows-only code; the Mac half's shared report passed on macOS arm64 (core_review_cycle_outgoing with control, 48 review-cycle rows). A Linux run of core_review_cycle_outgoing is recorded as not run from this station."}}]
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

## Windows half: read the link — bpr-main-win, 2026-10-01

Windows 11 Home 10.0.26200, MSVC 19.51, `build/windows-dev` Debug, at
`3ee80cb3` and later `main`, with the Mac half (`e464f13d`) in.

**The change** is in the Windows branch of
`src/adapters/workspace/src/local_workspace_source.cpp`. `read_link` no longer
answers `unsupported` for every link. It reads the link's own reparse data
with `FSCTL_GET_REPARSE_POINT`, on the handle `open_leaf` opened with
`FILE_FLAG_OPEN_REPARSE_POINT`, so the link is never followed. The rest
matches POSIX:

- it accepts only `IO_REPARSE_TAG_SYMLINK` with `SYMLINK_FLAG_RELATIVE`;
- it takes the substitute name, the target the system itself follows, and
  bounds-checks it inside the returned bytes;
- it turns `\` into `/` and converts to UTF-8 with the file's own `narrow`;
- it applies POSIX's `maximum_link_bytes` bound.

An absolute or device target, or any other reparse tag, stays `unsupported`.
That is not a repository link. Since the Mac half, the preview reports such a
path as unread rather than dropping it. `REPARSE_DATA_BUFFER` belongs to a
kernel header, so its documented symbolic-link layout is spelled out locally.

**Checks on Windows:**

| Row | Result |
|---|---|
| `review_cycle_candidate` | **pass**, 12.79 s, with its link case. This machine makes the link, and Git reads `docs-link` back from the candidate as mode `120000` with blob `README.md`. Before this change the same row failed at `a link is its target bytes, never followed` (the observation above): the negative control |
| `workspace_source`, `review_cycle_capture`, `review_cycle_outgoing`, `review_cycle_publication`, `review_cycle_publisher` | pass |
| `core_review_cycle_outgoing`, the Mac half's `unread_final_paths` among its cases | pass, 0.04 s |
| `core_review_cycle_material`, `architecture_boundaries` | pass |

The change is Windows-only code. The Mac half's results on macOS stand, and
its POSIX `readlinkat` path is unchanged. A Linux run of
`core_review_cycle_outgoing` was named above as owed. It covers the Mac
half's shared code, and is recorded here as not run from this station.

## Linux — 2026-10-01

Run by bpr-main-win through `wsl` in the bpr-main-linux checkout
`/home/mmarcin/BetterPullReview`, which was offline, under the owner's
authorization relayed by the coordinator. Nothing was committed from WSL.
Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, Qt 6.11.2, `build/dev`
Debug, at `aac225a4`. `cmake --build --preset dev` printed no `warning:`
line. Rows ran serially through `ctest`, offscreen, with `DISPLAY`,
`WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset, and with fresh `HOME`
and XDG roots. No set added a `/tmp` entry. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\linux\`, and are not
committed.

These are the Linux runs this card named as owed:

- `core_review_cycle_outgoing` passes, 0.01 s, the Mac half's
  `unread_final_paths` case among them;
- `review_cycle_candidate` passes, 1.07 s, its link case read with
  `readlinkat` as before.
