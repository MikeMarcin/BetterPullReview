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
