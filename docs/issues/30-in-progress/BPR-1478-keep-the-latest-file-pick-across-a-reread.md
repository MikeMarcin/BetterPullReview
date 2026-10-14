---
schema: "implement-rfc.card.v1"
id: "BPR-1478"
title: "Keep the reviewer's latest file pick when a reread lands"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1475"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1478--BPR-1478--1180ae81b4b9.json","origin":"BPR-1475 finding, owner request 2026-10-13"}
---

# Keep the reviewer's latest file pick when a reread lands

BPR-1475 made the window reread the pull request by itself after it
publishes. It found that a file the reviewer picks just as a reread of the
shown pull request lands can be lost: the window goes back to the file it
showed before the read. The start-review `pending` course failed 2 runs in 7
until it was changed to wait for the reread before choosing another file. A
manual Refresh has the same exposure.

## Acceptance

- AC1: A fixture course reproduces the loss deterministically: it starts a
  refresh, picks another file before the read lands, then lets the read land.
  On the code before this card the window returns to the earlier file.
- AC2: The reviewer's latest pick wins. A reread or Refresh never replaces a
  file chosen after it started with the view the window showed before it. The
  core makes that decision, and core tests cover it; AC1's course is the
  regression test that fails on the old code.
- AC3: The start-review `pending` course no longer needs to wait for the
  single comment's reread before it chooses another file, and passes
  repeatedly without that wait.
- AC4: The focused tests, format, changed-line tidy and the documentation
  gate pass, and the composition contract is resealed.

## Evidence

- Cause: a file picked in Review reaches the workbench at once, but the
  active tab takes it only on a later turn (`observe_tab_changes`, from a
  posted `schedule_tab_presentation`). A reread or Refresh accepted in between
  observes the new snapshot's files, which retires the workbench's selection.
  BPR-1303's `represent_refreshed_tab` then reloads the active tab's recorded
  view (`core::reload_active`), so the window went back to the earlier file
  and line.
- AC1: `application_refresh_pick_contract` (`--refresh-pick-course refresh`
  and `reread`) opens `src/engine/renderer.cpp` in Review and chooses a line.
  It starts the read from the public Refresh, or from
  `reread_after_publication` as a publication does, and holds its answer on
  the window's read lane. It holds the window's next tab turn, picks
  `src/engine/renderer.h` in the files drawer, releases the read, and
  releases the tab turn once a new snapshot is accepted. The order is the
  pick, then the read, then the tab, in every run; `Raced` checks that the
  active tab still named renderer.cpp and the workbench had no selection
  when the read landed. Without the window reporting the pick (the new core
  rule then never applies, so this is the old behaviour) 10 of 10 runs
  failed `pick-lost` with evidence `100`. Settled on renderer.cpp.
- AC2: `core::observe_file_choice` (`src/core/include/bpr/core/document_tabs.hpp`)
  keeps a file the reviewer chose that the active tab has not taken
  (`document_tabs_state::Chosen`, never persisted). The active tab's next load
  presents it at the file's initial position: a refresh's `reload_active`,
  or a comparison accepted for the tab when the head moved. Switching,
  closing or restoring tabs, or the tab taking the file, retires it. The
  window reports file gestures in Review (`dispatch_workbench_intent`, after
  the workbench accepted them) and a pasted link's file.
  `core_document_tabs` gains three tests: a refresh presents a later choice
  and no other tab changes; the choice gives way to the tab taking it and to
  later tab gestures; a comparison's load presents it, so it cannot outlive
  that load. With the final rule, 30 of 30 runs over both courses passed, 12
  of them on the final binary. An earlier version of the rule passed 19 of
  21: the two others expired the 40 s run budget before their first file was
  revealed, at the same moment as two start-review runs, while the load
  average was above 60.
- AC3: BPR-1475's wait in the `pending` course (`RereadOwed`, `RereadQueued`,
  `PendingPullRequest`) is removed. With the fix it passed 20 of 20 runs (12,
  then 8 on the final binary), after a first batch of 14 in which 12 passed
  and two expired with the runs above. The start-review contract's `pending`
  and `finish` courses pass. Without the fix it also passed 14 of 14 at this
  load: the timing flake is rare here, which is why AC1's course pins the
  order instead.
- AC4: passing on the merged tree (main e145ccd35): `core_document_tabs`,
  `core_review_cycle_tabs`, `application_document_tab_host`,
  `application_first_tab_comparison`, `application_threaded_reply_contract`,
  `application_label_authoring_contract`,
  `application_published_file_switch_contract`,
  `application_refresh_pick_contract`, `application_reply_draft_contract`,
  `application_profile_window_review_restoration_public_path`,
  `application_document_tabs_restoration_public_path`,
  `architecture_boundaries`, the start-review `uncertain` and `again`
  courses, and `application_review_round_trip_composition_contract` after
  the reseal. That reseal also covers main's BPR-1479 change to
  `github_application.cpp` (5f6be7e91, 392256fbc), which main had not
  resealed. `_coding-standard/scripts/format.py` leaves the touched files
  unchanged. Changed-line tidy is clean: its nine warnings in
  `github_application.cpp` are all on earlier lines. The documentation gate
  reports `DOCUMENTATION_OK`. `docs/ux/workbench-specification.md` states
  the rule. The new row is pinned `dos-volume-name` in
  `scripts/verify_test_network_isolation.py` by the file-switch row's shape,
  not yet measured contained on Windows.
- Known failures outside this slice: the start-review `refused` course fails
  `refusal-lost-the-draft`, as BPR-1475 recorded on main.
  `test_network_isolation_manifest` fails because main registers six
  `application_source_pr_*` tests `WINDOWS_UNCONTAINED` without pins, and
  because this build lacks the gh CLI transport test (the full build stopped
  when the disk filled); it raises nothing for the new row.
- Not covered: a line chosen while a refreshed tab's reload is still being
  presented. The reading position of the view shown before the reload is
  refused as stale, and the reload restores its recorded line. This was
  found by reading the code, not reproduced.
