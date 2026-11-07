---
schema: "implement-rfc.card.v1"
id: "BPR-1927"
title: "Split pull_request_overview_page.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"5708dd5f2","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split pull_request_overview_page.cpp by responsibility

Round 13 of the largest-files refactor.
`src/adapters/qt/src/pull_request_overview_page.cpp` is 6,139 lines: about
1,000 lines of colour, chip and description-view helpers, then the
out-of-line members of the overview's file model, row delegate, participant
and request rows, role line and faces, and about 160 members of
`pull_request_overview_page` itself.

## Acceptance

- AC1: Move the definitions, unchanged, into responsibility-named sources
  beside it; helpers used by one source stay private to it, shared ones are
  declared in one support header. The page's constructor stays in
  `pull_request_overview_page.cpp`; the public headers do not change.
- AC2: Preserve behavior: Qt adapter and app build in both configurations;
  every overview row (Qt page, chips and folder rows at 125 and 200 percent,
  read marks, application and public-path courses), the workbench rows, and
  the architecture and format gates.
- AC3: Land the change, the completed card and a journal line on main.

## Result

Source `ee22d8558`; `pull_request_overview_page.cpp` goes from 6,139 to 1,036
lines and now holds the page's 843-line constructor. New files (lines):
`pull_request_overview_people_and_status.cpp` 898 (role line, faces and the
Status drawer's attention, review, people and status sections),
`pull_request_overview_presentation.cpp` 894 (presenting a projection, checks
and cached content), `pull_request_overview_files_interaction.cpp` 841 (file
keys, menus, the event filter, moving between files),
`pull_request_overview_file_model.cpp` 803,
`pull_request_overview_delegate_and_rows.cpp` 691 (row delegate painting,
participant and request rows), `pull_request_overview_chips_and_details.cpp`
650 (comment chips, labels, merge details, the description editor),
`pull_request_overview_support.cpp` 589 and
`pull_request_overview_support.hpp` 241. All are under 1,100.

Checks on macOS arm64: Qt adapter and full tests-off builds; full dev build
except `main`'s broken `agent_context_supplement_tests.cpp`; and 46 rows
(every overview row, the workbench rows, architecture and coding): 41 pass.
The five failures do not come from this split:
- `coding_format_check` flags only `tour_agent_panel.cpp`, which BPR-1925
  landed unformatted.
- `qt_workbench_window_scale_100`, `_125` and `_200` ("the Comments tab
  shows the notes with Only new, Hide resolved and Next new on the tab row")
  and `application_accepted_file_in_overview` ("nothing is read or accepted on
  a first visit") fail with the same messages when the Qt adapter is rebuilt
  from the pre-split source. They already failed in this refactor's earlier
  baseline runs.

Windows and Linux were not built.
