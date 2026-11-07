---
schema: "implement-rfc.card.v1"
id: "BPR-1920"
title: "Split workbench_window.cpp by responsibility"
status: "Ready"
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
