---
schema: "implement-rfc.card.v1"
id: "BPR-0736"
title: "The review queue ignores Return on macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_by":"BPR-0733","mandate":"attended-prep-2026-09-10 r7"}
---

# The review queue ignores Return on macOS

Found while landing [BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
On macOS `QAbstractItemView` hands Return and Enter to its edit trigger and
never emits `activated`; probed empirically on the recently viewed list
(`activated=0`, event ignored, for Return, Enter and viewport delivery).
The recently viewed list therefore reads both keys in
`workbench_window::eventFilter` and behaves the same on every platform.

The review queue's keyboard path (`src/adapters/qt/src/workbench_window.cpp`,
the `QAbstractItemView::activated` connection near the queue view's
construction, around line 1869) still relies on `activated`, so a
keyboard-only reviewer on macOS cannot open a queue row with Return.
Pre-existing; `qt_composed_github_scale_*` drive the queue through the
signal, not through a key event, which is why it passed.

## Scope

- The queue view opens its current row on Return and Enter on every
  platform, by the same mechanism the recently viewed list uses (an event
  filter or a shared helper), without changing mouse activation.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp`, its header,
  `tests/adapters_qt/composed_github_tests.cpp`.

## Acceptance

- `qt_composed_github_scale_{100,125,200}` send `Qt::Key_Return` to the
  queue view with a row current and observe the open dispatch; the
  existing activation path is unchanged.
