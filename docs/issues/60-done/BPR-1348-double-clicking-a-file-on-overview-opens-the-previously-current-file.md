---
schema: "implement-rfc.card.v1"
id: "BPR-1348"
title: "Double-clicking a file on Overview opens the previously current file in Review"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1339 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1339-overview-accepted-mark-v002 Accept note","decision_id":"e675d148-d02a-45b9-b234-370b91f65729","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1339","BPR-0647","BPR-0656","BPR-0658"],"acceptance":["BPR-1348:reproduced","BPR-1348:fixed"]}
acceptance: [{"criterion":"BPR-1348:reproduced","outcome":"pass","revision":"e814554a3","evidence":{"kind":"command","path":"tests/end_to_end/overview_double_click_public_path_tests.cpp","detail":"application_overview_double_click on conventional-comments-v1 opens renderer.cpp via reveal_fixture_file as the launcher did, returns to Overview and double-clicks src/main.cpp by pointer; with workbench_window.cpp at its parent it fails 'Review shows src/engine/renderer.cpp'. Cause: enter_review_at selected the file, then chose Review, whose modality intent schedules the active tab's presentation (BPR-1132)."}},{"criterion":"BPR-1348:fixed","outcome":"pass","revision":"e814554a3","evidence":{"kind":"command","path":"tests/end_to_end/overview_double_click_public_path_tests.cpp","detail":"Review chosen first, then the file. The course opens src/main.cpp, README.md and src/engine/shaders/post/bloom.frag by double-click and src/main.cpp by Return; qt_workbench_window_scale_{100,125,200} require the selection after Review's modality intent (fail at the parent). application_accepted_file_in_overview, application_overview_summary_{1,2}, application_setup_destination_public_path, architecture_boundaries, test_network_isolation_manifest pass. macOS; Linux and Windows not run."}}]
---

# BPR-1348: Double-clicking a file on Overview opens the previously current file in Review

## Owner note

In attended session `0d079940affe6da5c393df07d82a22d8` the owner accepted
[[BPR-1339]]'s Overview mark (`BPR-1339-overview-accepted-mark-v002`, decision
`e675d148-d02a-45b9-b234-370b91f65729`, event 12, 2026-10-09T04:12:56Z) with
this note, verbatim:

> bug: I double clicked main.cpp and it opened to renderer.cpp in the review.

Two attachments. The first is Overview on `bpr-fixtures/nested-folders#21`
("Move renderer, shaders and widgets into nested folders") with the
`src/main.cpp` row selected, while the review bar says "File 5 of 17" and the
status bar "Actionable file 5 of 17. Private progress: unread, accepted.":
`src/engine/renderer.cpp`, the file the frozen build opened in Review, still
current. The second is Review right after the double-click, showing
`renderer.cpp` with its row current in the Files drawer.

## Cause

Confirmed. The double-click reached the right row: `open_file_at` dispatched
`src/main.cpp`, and `workbench_window::enter_review_at` found it and sent
`select_workbench_file_intent` for it. It then chose Review. Choosing Review
sends `set_workbench_modality_intent`, and the composition answers that with
`schedule_tab_presentation()` ([[BPR-1132]]: the comparison shows the active
tab's pair). The active tab was `renderer.cpp`, which the launcher had opened
from the Files drawer, so the scheduled presentation replaced the selection
that had just been made. The conversation pick ([[BPR-1293]]) already chose
Review first for this reason ("a file gesture that follows the modality wins
over the active tab's restoration"); `enter_review_at` did it the other way
round. It was never about double-click: Return on a row, Start review,
Continue review and a thread's Back to the code all go through
`enter_review_at`. It shows only while a tab is open, which is why
`application_accepted_file_in_overview`'s Enter (no tab, files opened by Next
file) passed.

## Fix

`enter_review_at` chooses Review first and selects the file after it
(`src/adapters/qt/src/workbench_window.cpp`).

## Acceptance

- `BPR-1348:reproduced`: a composed course on the frozen build's fixture
  opens `renderer.cpp` in Review as the launcher did, returns to Overview and
  double-clicks `src/main.cpp`'s row by pointer; against the code before the
  fix Review shows `renderer.cpp`.
- `BPR-1348:fixed`: the same course shows `src/main.cpp`, and a root file and
  a deeply nested file open the same way; Return on a row still opens it.

## Verification

macOS arm64, `build/dev` in a worktree, offscreen, at `e814554a3` (the fix
rebased onto `2779ac75a`; first run on `dc2ef601a` over `d38b72965`):

- `application_overview_double_click` (new,
  `tests/end_to_end/overview_double_click_public_path_tests.cpp`, the frozen
  build's `conventional-comments-v1` fixture): opens `renderer.cpp` with
  `reveal_fixture_file`, as the launcher did, then on Overview double-clicks
  `src/main.cpp`, `README.md` and `src/engine/shaders/post/bloom.frag` by
  pointer (press, release, double-click, release on the row's name) and
  presses Return on `src/main.cpp`; each opens in Review. **Failing-before
  control:** with `workbench_window.cpp` at its parent the course fails at
  "double-clicking src/main.cpp on Overview opens it in Review, not
  src/engine/renderer.cpp (Review shows src/engine/renderer.cpp)".
- `qt_workbench_window_scale_{100,125,200}`: Start review, Continue review and
  an activated file row must select their file after Review's modality
  intent. **Failing-before control:** at the parent it fails at "Start review
  selects the entry file through the core, after entering Review".
- Also passing: `application_accepted_file_in_overview`,
  `application_overview_summary_{1,2}`,
  `application_setup_destination_public_path`, `architecture_boundaries`,
  `test_network_isolation_manifest` (the new row pinned `dos-volume-name` in
  `scripts/verify_test_network_isolation.py`, by the shape of the
  accepted-file row; not yet measured contained on Windows).
- clang-format clean on the changed lines and the new file.

Windows and Linux have not run this change; it adds no platform code.
