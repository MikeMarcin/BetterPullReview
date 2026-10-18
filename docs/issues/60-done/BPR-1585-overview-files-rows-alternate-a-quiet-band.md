---
schema: "implement-rfc.card.v1"
id: "BPR-1585"
title: "Overview's file rows alternate a quiet band within each folder"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"Would the Overview files view benefit from a subtle alternating background for rows?","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0647","BPR-1089","BPR-1213","BPR-1363","BPR-1529"],"reservation":"docs/issues/_reservations/BPR-1585--BPR-1585--843eb7b989aa.json","requested_skill":"frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5a580a955","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","detail":"test_file_rows_band_within_each_folder (qt_pull_request_overview_page, folder_rows_125/200): every shown row's ground follows the rule across every folder open, docs/api and root closed (later groups unchanged), collapse/expand all, a hidden row, a reversed sort and a refresh; headers and Not on a file on the ground; selected rows are the selection. Negative control: Qt whole-tree alternation in the band colour fails the rule."}},{"criterion":"AC2","outcome":"pass","revision":"5a580a955","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","detail":"test_band_keeps_every_ink_readable: step 1.08 in Light, Dark, HC Light, HC Dark; text, +/- lines, chip words, A/D/M letters >= 4.5:1 and badges, changed chip, read marks, unknown mark, twelve label tints >= 3:1 on the band where they hold on the ground; pre-existing shortfalls (placeholder ink, amber R badge and bubble, open digits) named on the card with both values."}},{"criterion":"AC3","outcome":"pass","revision":"5a580a955","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","detail":"Not on a file's operation, Changed and Read cells paint nothing; its name and chips paint."}},{"criterion":"AC4","outcome":"pass","revision":"5a580a955","evidence":{"kind":"command","detail":"Full ctest --preset dev on merged trees c401a1378 and e786531c9: all non-timing failures pass on rerun; large-review budgets fail only under load and pass at load 20 with an A/B showing no gap change; documentation_structure fails only on the branch NEXT_ID lag and verify_documentation passes on main. 30 focused rows pass at 5a580a955; clang-format 22.1.8 clean; clang-tidy clean on pull_request_overview_page.cpp; DOCUMENTATION_OK."}},{"criterion":"AC5","outcome":"pass","revision":"3ea341dd2837447e7c636188b136dc894efb139f","evidence":{"kind":"observation","detail":"Owner Accept, no note, attended session f1a8896a19f1fb037abd90b45bd11512, decision 545cba0f-a1e9-41b1-9a63-074b1d45d6e9 (event 4, handed back at 5), candidate BPR-1585-files-band-v001 fingerprint 925175e7, frozen root bpr1585-files-band-v001.z0knvg2u."}}]
---

# BPR-1585: Overview's file rows alternate a quiet band within each folder

The owner asked on 2026-10-17: "Would the Overview files view benefit from a
subtle alternating background for rows?" The Files table is wide (File,
Added, Removed, Changed, Read, Comments, Note) and the gap between a file's
name and its far columns is large, so the eye loses its row. The table
turns alternating colours off today (`setAlternatingRowColors(false)`,
[[BPR-0647]]), and Qt's own alternation counts every visible row of the
tree, so it cannot restart at a folder.

Governing design: the Overview page of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), with
[[BPR-1213]]'s flat folder rows, [[BPR-1363]]'s steady columns, [[BPR-1089]]'s
chips and Not on a file row and [[BPR-1529]]'s folder ink unchanged.

## Acceptance

- AC1: File rows alternate between the table's ground and a quiet band
  derived from the theme, counted over the rows on screen within each
  folder: the first file under a folder header is on the band, the second
  on the ground, and so on. Folder header rows and the Not on a file row are
  never banded, so every group starts the same way. Collapsing or expanding
  a folder, a sort and an incremental refresh keep the rule, and a hidden row
  takes no turn. A render check samples the band on visible rows across
  collapse and sort, with a negative control that fails on Qt's
  whole-tree alternation.
- AC2: In Light, Dark, High Contrast Light and High Contrast Dark the band
  is visible against the ground and quiet (a luminance ratio between 1.03
  and 1.25), file text, line counts and chip digits stay at 4.5:1 or more on
  it, and the operation badges, read marks and comment marks at 3:1 or more.
  Selection, keyboard focus and the chip ring draw over the band exactly as
  over the ground. Any ink that is below these floors on the ground today is
  named on this card with its value on both.
- AC3: The Not on a file row draws only its name and its Comments chips: no
  operation badge, Changed mark or read mark, which it drew from empty roles
  as an Added "A", a "?" and an unread ring.
- AC4: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark
  before and after: "The files table's quiet alternating band helps follow a
  row across, and folders still read as groups."

## Implementation

Load frontend-design:frontend-design first. Qt presents: the band is the
row delegate's (`overview_row_delegate`), as the cell's background brush and
as the ground the cell's marks mix with; no QSS, no new appearance role, no
core, host, provider or storage change. Only the Files table is touched;
the Overview's toggle, loading states, disclosure colours, avatars and side
panel belong to other cards. Never drive or capture the owner's desktop;
renders are offscreen and the owner checks the frozen build themselves.

## Checkpoint — candidate v001, 2026-10-17

Product candidate `3ea341dd2837447e7c636188b136dc894efb139f` on branch
`bpr-1585-files-band` (worktree
`/Users/jaeger/.codex/worktrees/bpr1585-files-band/BetterPullReview`),
baseline `007b8f093`. Not merged to main until the owner accepts. Exact
paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1585/handoff.json).

Design (frontend-design loaded first). The band is
`overview_row_band`: the palette's `Base` moved toward its `Text` until the
two stand 1.08:1 apart (`overview_row_band_contrast`). It is neutral, so it
never reads as the blue selection or a state, it follows any theme or
custom profile, and it does not borrow `AlternateBase`, which is the code
current-line tint a user may set loud for code ([[BPR-1513]] kept it out of
chrome for the same reason). Values: Light `#f6f6f7` on `#ffffff`, Dark
`#161a1f` on `#0d1117`, High Contrast Light `#f6f6f6` on `#ffffff`, High
Contrast Dark `#0d0d0d` on `#000000`. High contrast keeps the band: text
stays above 15:1 on it, and following a row across a wide table is hardest
for exactly the people who choose those themes; it is the same quiet step,
not a stronger one.

Folders: a header row is on the ground and starts its group, and the files
under it take turns from the band. So every group reads header, band,
ground, and a folder of one file still shows its file as a row apart from
its header; a header's own ink, weight, disclosure and count mark it at a
group's end. The turn is the view's own geometry (uniform rows): the rows
shown between the file and its header, so a collapse, a sort or a hidden row
moves no other group, at constant cost (3,000 files in one folder: 8 ms a
full debug repaint at the bottom against 72 ms for a walk over siblings).
A banded cell's `Base` is the band, so the M tile mixes with it and the
A/D/R letters and chip words cut to it. A selected row is the selection's.

The Not on a file row ([[BPR-1089]]: unanchored threads, pull request
comments and review summaries) is intended. Its "A" badge, "?" and unread
ring were a bug: it is not a file, but the delegate painted its operation,
Changed and Read cells from the roles' empty values (operation 0 is added,
change 0 is unknown, mark 0 is unread). It now draws its name and its chips
only (AC3). The round mark with a bar in the owner's screenshot is the
Comments cell's ? qualifier in its not-resolvable form: pull request
comments and review summaries cannot be resolved, so Open counts them under
? (BPR-1089 R24). That is intended.

| Acceptance | Evidence at `3ea341dd2` | Result |
|---|---|---|
| AC1 | `test_file_rows_band_within_each_folder` (qt_pull_request_overview_page; folder_rows_125/200): every shown row's ground follows the rule with every folder open, docs/api and the root closed (later groups unchanged), all closed and reopened, a hidden row, a reversed sort, a refresh; Not on a file and headers on the ground; the selected banded and unbanded rows are the selection. Negative control: Qt's alternation in the band colour draws every other shown row and fails the rule | pass |
| AC2 | `test_band_keeps_every_ink_readable`: step 1.08 in all four themes; text, +/− lines, chip words, A/D/M letters at 4.5:1 and badges, changed chip, read marks, unknown mark and the twelve label tints at 3:1 on the band wherever they hold on the ground | pass |
| AC3 | the same test: Not on a file's operation, Changed and Read cells paint nothing; its name and chips paint | pass |
| AC4 | (see Closeout) focused 29 rows (overview page, chips and folder rows 125/200, read marks, workbench overview, workbench window 100/125/200, composed GitHub 100/125/200, files tree, nested folders and overview public paths) pass at the candidate (qt_workbench_window_scale_200 hit the runner's child timeout once at load average 250 and passed alone in 51 s); clang-format 22.1.8 clean; clang-tidy clean on `pull_request_overview_page.cpp` and the new test code; DOCUMENTATION_OK. The full `ctest --preset dev` is owed on the merged tree | pass |
| AC5 | attended session `f1a8896a19f1fb037abd90b45bd11512`, candidate `BPR-1585-files-band-v001` (fingerprint `925175e7`), frozen root `/Users/jaeger/bpr1585-files-band-v001.z0knvg2u`: owner Accept, decision `545cba0f` | pass |

Inks already short on the ground, named as AC2 asks (ratio on the ground,
then on the band): the placeholder ink of Note and the unknown "?" is 3.16
then 2.92 Light, 4.15 then 3.83 Dark, 4.0 then 3.7 High Contrast Light;
the amber of the R badge, its letter and the unresolved bubble is 2.52 then
2.33 Light and High Contrast Light; the amber open-chip digits are 3.77 then
3.49 Light. Folder paths use the placeholder ink too but are never banded.
Each loses no more than the band's step. Raising those inks is a separate
decision, not this card's.

Candidates, not changed here: Review's file tree (`files_tree_delegate`),
the Threads and Discuss lists and the Overview's Check details list are
other components; Review Cycle's notes list already alternates with Qt's
own colours.

## Closeout — accepted and landed, 2026-10-17

The owner accepted `BPR-1585-files-band-v001` with no note in attended
session `f1a8896a19f1fb037abd90b45bd11512` (decision
`545cba0f-a1e9-41b1-9a63-074b1d45d6e9`, event 4, handed back at event 5),
on the frozen build of `3ea341dd2`. The branch landed on main as
`5a580a955`.

Merged-tree verification:

- Full `ctest --preset dev` on the merged tree `c401a1378` (main
  `26db2cfff` plus the candidate): 757 of 772 passed in the first pass at
  load average 120–300. On serial rerun, 9 of the 15 failures passed:
  rfc0016_production_identity_security (it had failed only on the
  untracked `.agents` link the worktree needed for the documentation gate),
  live_mutation_isolation, application_diff_host,
  release_update_native_https, large_review_replay_contract, the agent
  materialization and cleanup contracts, application_epoch_ordinary_update_recreated
  and application_private_review_native. The five large-review
  benchmarks failed only on timing budgets while the machine was loaded
  (composition startup at 2 to 5 times its 1 s budget, before any pull
  request opens); at load average 20 all five passed. An A/B of the
  paged-load input gap, four runs each, gave 83.8–85.7 ms with the band
  and 82.5–99.2 ms with it stubbed out. documentation_structure failed
  because the branch's `NEXT_ID` trails the high water other branches
  have since reached; `verify_documentation.py` on main passes
  (DOCUMENTATION_OK).
- A second full run on the re-merged tree `e786531c9` (main `c29d90e19`,
  with BPR-1583's change to the same file) gave 758 of 774. Its
  non-timing failures (application_diff_host,
  application_anonymous_public_preview_unavailable,
  application_threaded_reply_contract, epoch_portable_occurrence,
  qt_hide_drawers_native_menu_key, epoch_ordinary_update_recreated,
  terminal_diagnostics, documentation_issue_graph_contract and
  issue_id_allocation_contract) passed on rerun. The large-review budgets
  failed again at load average 80, and documentation_structure failed on
  the same `NEXT_ID` lag.
- On the landed tree `5a580a955`, 30 focused rows pass: the overview page,
  chips and folder rows at 125/200%, read marks, workbench overview,
  workbench window 100/125/200, composed GitHub 100/125/200, the files
  tree, nested-folders and overview public paths, and close review.
  clang-format 22.1.8 is clean. DOCUMENTATION_OK on main.

The review server is stopped, protection is released, and the frozen root
is removed.

