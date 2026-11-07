---
schema: "implement-rfc.card.v1"
id: "BPR-1920"
title: "Split workbench_window.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"aff74307f","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split workbench_window.cpp by responsibility

Round 8 of the largest-files refactor. `src/adapters/qt/src/workbench_window.cpp`
is about 21,500 lines: roughly 900 lines of anonymous helpers and some 800
`workbench_window` member functions already defined out of line against the
public `workbench_window.hpp`.

## Acceptance

- AC1: Move the member definitions, unchanged, into responsibility-named
  sources beside it; helpers used by one source stay private to it, and the
  shared ones are declared in one support header. The public header does not
  change.
- AC2: Preserve behavior: Qt adapter and app build in both configurations; the
  workbench window, composed GitHub and composition courses; architecture,
  format and documentation gates.
- AC3: Land the change, the completed card and a journal line on main.

## Result

Source `543e4e210`; `workbench_window.cpp` goes from 21,476 to 340 lines (its
remaining helpers and static assertions). New files (lines): `workbench_window_central_surface.cpp` 2119, `workbench_window_queue_controls_and_menus.cpp` 1838, `workbench_window_home_landing.cpp` 1791, `workbench_window_defects_and_home_rows.cpp` 1764, `workbench_window_settings_and_layout.cpp` 1755, `workbench_window_github_presentation.cpp` 1750, `workbench_window_repositories_and_files.cpp` 1737, `workbench_window_comments_and_threads.cpp` 1722, `workbench_window_home_views.cpp` 1707, `workbench_window_drafts_and_behaviour.cpp` 1699, `workbench_window_header_and_queue_settings.cpp` 1562, `workbench_window_replies_and_review_composition.cpp` 1525, `workbench_window_construction.cpp` 860, `workbench_window_support.cpp` 352, `workbench_window_support.hpp` 207.

One file exceeds the 2,000-line ceiling: `workbench_window_central_surface.cpp`
(2,119) is the single 1,873-line `build_central_surface` plus its includes;
going lower means splitting that function, which is a design change. The public
`workbench_window.hpp` is unchanged. New files keep the original's include
vocabulary, because `architecture_boundaries` lets the Qt adapter reach the
application GitHub surface only through `github_projection.hpp`; a first cut
included a declaring header directly and that rule caught it.

Checks on macOS arm64: Qt adapter and composition library tests-off builds; full
dev build except `agent_context_supplement_tests.cpp`, which `main` broke
(BPR-1916 added a 17th field to `agent_context_supplement`; the test still
binds 16); and 230 rows (every `qt_*` row, architecture, both composition
contracts, the diff workbench, workbench composition and UIforETW courses): 221
pass. The nine others fail identically at `main` `aff74307f` built separately:
`qt_workbench_window_scale_*`, `qt_drawer_surfaces_fit_scale_*`,
`qt_review_tour_steps`, `qt_feedback_label_settings` and
`qt_current_file_unread_badge`. Windows and Linux were not built.
