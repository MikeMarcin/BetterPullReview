---
schema: "implement-rfc.card.v1"
id: "BPR-0875"
title: "Windows: an unnamed shutdown cleanup failure fails thirteen rows that had already passed everything they measure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0875: Windows: an unnamed shutdown cleanup failure fails thirteen rows that had already passed everything they measure

## Defect

Thirteen rows fail on the Windows station **only on cleanup**. Twelve report
`PUBLIC_PATH_FAILED` with `cleanup_failed=1`, and **eleven of those carry
`presentation=1`** — every single thing the course set out to measure passed,
and then shutdown failed:

`application_uiforetw_selected_diff_contract`,
`application_workbench_composition_contract`,
`application_review_round_trip_composition_contract`,
`application_overview_since_review_public_path`,
`application_review_behaviour_contract`,
`application_saved_server_public_path`,
`application_appearance_theme_customization`,
`application_epoch_persistence_github-com`,
`application_epoch_persistence_ghes-3_18`,
`application_epoch_maintenance_public_path`,
`application_appearance_gesture`,
`application_appearance_store_fallback`, plus
`application_native_notification_attended_contract`, which refuses on the same
fact after printing `DISPLAY_BOUNDARY_OK`, `QT_ACTIVE_WINDOW_OK`,
`TRAY_SIGNAL_OK`, `ACTIVATION_OK` and `EXACT_LANDING_SELECTION_OK` with a
correct payload.

[[BPR-0860]]'s fix is confirmed working on Windows inside this cluster:
`WORKBENCH_DRAWER_WIDTH_WIRE_OK` and every other workbench marker print, the
composition trace and host compose, `presentation=1`, and the row still fails
at `PUBLIC_PATH_STAGE:23` on `cleanup_failed=1` after 5711 ms of its 40 s
budget. So this one fact is masking a large amount of working behaviour.

**`ShutdownCleanupFailed` has three sources** in
`src/app/src/github_application.cpp` (~11762-11812): the durable layout write,
the layout erase, and `remove_all(McpSnapshotRoot)`. **None is named in the
output.** Thirteen rows therefore report the same undifferentiated fact, and
finding which of the three fired needs a bisect rather than a run.

This is the run's first full Windows sweep, so the cluster is newly observed.
Whether it is newly broken is unknown and is part of the work.

## Scope

- **Name the three sources first.** Give each its own term with the path and
  the error it saw. One run should then say which of the three fires, on all
  thirteen rows at once, and it is plausible they do not all share one source.
  Do this before attempting a fix: the cluster is large enough that guessing
  costs more than instrumenting.
- Then fix the cause. A Windows-specific `remove_all` failure on a directory
  still held open is the obvious first suspect, and it is the same family as
  [[BPR-0868]] (the contained temporary helper swallowing a removal error) and
  the `tmp_pack_*` READONLY|ARCHIVE hazard this run already recorded.
- Establish whether the cluster predates this run.
- Check whether a person quitting the real application on Windows hits the
  same failure. Thirteen automated rows failing at shutdown is worth knowing
  about, but a reviewer losing their layout on quit would matter far more, and
  the durable layout write is one of the three candidates.

## Acceptance

- A cleanup failure names which of the three sources failed, and with what
  error, shown in a real run.
- The thirteen rows pass on Windows, or each remaining failure is a named,
  understood cause with its own record.
- The card states whether a person quitting the application on Windows was
  ever affected.
- macOS and Linux are unaffected: all thirteen pass there today.

## The open question is answered: a person is affected too (r5 critical review)

This card asked whether a person quitting the real application on Windows hits
the same failure. **They do, and here is the line:**
`src/app/src/profile_desktop.cpp:851-853` sets `ExitCode = 1` when
`profile_shutdown_cleanup_failed()`, in the same drain the real desktop uses.

So the Windows cluster is not test-only. A reviewer quitting the application
gets exit 1 from the same three unnamed sources at
`src/app/src/github_application.cpp:11762-11812`, and one of those three is
the durable layout write — which is the one whose failure a person would
actually notice, because it is their window layout.

That raises this card's stakes: it is not thirteen red rows, it is thirteen
red rows plus a user-visible failure sharing one unnamed cause. Naming the
three sources is still the right first step, and now it is worth doing for the
product's sake rather than only the suite's.

Related: [[BPR-0890]] records that a crash *after* the marker passes any row
with a `PASS_REGULAR_EXPRESSION`, so shutdown faults are exactly the class the
suite is currently weakest at seeing. The two should be read together.
