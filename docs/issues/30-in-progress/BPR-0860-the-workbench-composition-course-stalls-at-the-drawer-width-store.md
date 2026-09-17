---
schema: "implement-rfc.card.v1"
id: "BPR-0860"
title: "The workbench composition course stalls at the drawer width store until its run budget expires"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0860: The workbench composition course stalls at the drawer width store until its run budget expires

## Defect

`application_workbench_composition_contract` fails on both platforms:

- on Linux at `a1864fa2`, with `RUN_BUDGET_EXPIRED at 40028 of 40000 ms`;
- on macOS at `20d99914`, with `RUN_BUDGET_EXPIRED … elapsed_ms=40020 stage=2`,
  then `WORKBENCH_DRAWER_WIDTH_WIRE_FAILED` and the later markers.

The probe drags `central_splitter()->move_handle(2, …)`
(`src/app/src/github_application.cpp` ~14881). It then waits in
`awaiting_width_store` (~14889-14906), which has no cap on its waits. Neither
the width dispatch nor the persisted layout arrives, so the course runs until
its budget ends. The later markers fail only because the probe never reaches
them.

[[BPR-0718]] fixed an earlier failure of this row. The scout named candidate
commits since 2026-09-11, each with low confidence:

- `ad0bb67d`: Dif in the AI tools area, in `workbench_window.cpp`;
- `3dcbf237`: BPR-0729's one-pane comparison, which changed the layout store;
- `ce62ed52`;
- `ed2b3f14`.

## Scope

- Find the introducing commit.
- Name whether handle 2 is still the files-drawer edge, and whether a drag
  still dispatches and persists the width.
- Fix the product or the probe at the cause.
- Cap the phase's waits so a stall fails by name before the run budget ends.
- A `src/app` change reseals the round-trip pins with a recorded review.

## Acceptance

- The row passes on macOS and Linux.
- A stalled width store fails with a named term, not by running out the
  budget.

## Second scout (dsp-004, read-only): what code reading rules out

**Ruled out.** The splitter order at HEAD is repositories, files, centre,
conversations (`workbench_window.cpp` ~1418), so handle 2 is still the
files-drawer edge. Every link from drag to store is intact:

- `move_handle`
- `splitterMoved`
- `observe_handle_drag` (~7844), which requires a published layout, an
  expanded files drawer and 4 widgets
- `observe_drawer_width`
- `dispatch_workbench_width` (`github_application.cpp` ~7394)
- `resize_now` (`workbench_application_host.cpp` ~1020)
- `submit_settings_store` (~770)
- `finish_settings_store` (~1455), or, on profile defaults, `accept_preferences` (~1167)

`ad0bb67d`, `3dcbf237` and `ed2b3f14` break no link.

**Remaining candidates**, all runtime conditions:

- (a) the files drawer cannot grow in the offscreen window, so the size is
  unchanged and nothing is dispatched;
- (b) the course is on the profile-defaults branch, and `ce62ed52`'s
  empty-Home defaults make `accept_preferences` mismatch, so the stored count
  never moves (a product regression);
- (c) no layout was ever published (`LayoutWidths` is empty).

A user dragging the drawer edge still persists the width, by the code, unless
(b) applies.

**Next step.** Cap the wait at ~14887-14896 and fail with separate terms for
"no width dispatch" and "no stored layout". Emit the splitter sizes, the
drawer state, `LayoutWidths` and whether the profile is scoped. Bisect from
the last green run.
