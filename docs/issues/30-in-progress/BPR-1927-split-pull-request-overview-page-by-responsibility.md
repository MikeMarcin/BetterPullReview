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
and request rows, role line and faces, and 164 members of
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
