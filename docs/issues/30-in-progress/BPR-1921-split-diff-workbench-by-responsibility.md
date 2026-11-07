---
schema: "implement-rfc.card.v1"
id: "BPR-1921"
title: "Split diff_workbench.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"f17e57017","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split diff_workbench.cpp by responsibility

Round 9 of the largest-files refactor. `src/adapters/qt/src/diff_workbench.cpp`
is 7,145 lines: a 2,538-line anonymous namespace (the source pane and thread
tick scroll bar widgets among its helpers) and 177 `diff_workbench` members
defined out of line against the public `diff_workbench.hpp`.

## Acceptance

- AC1: Move the members, unchanged, into responsibility-named sources beside
  it; widgets and helpers used by one source stay private to it, shared ones
  are declared in one support header. The public header does not change.
- AC2: Preserve behavior: Qt adapter and app build in both configurations; the
  diff workbench, composed GitHub and selected-diff courses; architecture,
  format and documentation gates.
- AC3: Land the change, the completed card and a journal line on main.

## Result

Source `a9fd5c1ba`; `diff_workbench.cpp` goes from 7,145 to 85 lines. New files
(lines): `diff_workbench_support.cpp` 1959, `diff_workbench_find_and_editing.cpp` 1724, `diff_workbench_marks_and_navigation.cpp` 1658, `diff_workbench_presentation.cpp` 1497, `diff_workbench_support.hpp` 807. All are under 2,000. The class's nested types that the source
defined out of line (`find_presentation`, `head_edit`, `staged_publication`,
`unified_presentation`, `geometry_measurement`) are in the support header,
because every member file needs them complete.

Checks on macOS arm64: Qt adapter and composition library tests-off builds; full
dev build except `main`'s broken `agent_context_supplement_tests.cpp`; and 231
rows (every `qt_*` row, architecture, both composition contracts, diff workbench,
diff workbench shutdown, workbench composition, UIforETW and native-appearance
selected-diff courses): 221 pass. `qt_hide_drawers_native_menu_key` failed under
parallel load and passes alone; the other nine fail identically at `main`
`aff74307f` (see [[BPR-1920]]). Windows and Linux were not built.
