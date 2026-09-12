---
schema: "implement-rfc.card.v1"
id: "BPR-0736"
title: "The review queue ignores Return on macOS"
status: "Done"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_by":"BPR-0733","mandate":"attended-prep-2026-09-10 r7"}
acceptance: [{"criterion":"AC1 Return and Enter open current queue row without disturbing activation","outcome":"pass","revision":"7e53ff78","evidence":{"kind":"command","detail":"New real QKeyEvent assertion failed before fix in build/logs/bpr-0736-d25-red-test.log. Final qt_composed_github_scale_100/125/200 passed in build/logs/bpr-0736-d25-green-tests.log. Both keys dispatch exactly selected PR 74; invalid selection dispatches nothing; existing mouse click still opens PR 73. Touched format, focused tidy and git diff --check pass."}}]
---

# The review queue ignores Return on macOS

Found while landing [BPR-0733](BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
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

## Candidate and direct check, 2026-09-12 (d25, mandate r16)

The queue view now installs the window's existing key event filter. Return
and Enter invoke the same `open_pull_request_item` semantic dispatch as the
existing activation connection, and the key is consumed so it cannot open
twice. An invalid current index opens nothing; mouse activation keeps its
existing connection and still opens the clicked row.

The composed Qt test sends actual `QKeyEvent` presses to the queue view,
asserts that each key opens exactly selected row 74, asserts no dispatch for
no selection, and then still clicks row 73 through the mouse path. The new
key assertion failed before the product fix
(`build/logs/bpr-0736-d25-red-test.log`) and all three
`qt_composed_github_scale_{100,125,200}` passed after it
(`bpr-0736-d25-green-tests.log`). Touched-file format, focused tidy
(`bpr-0736-d25-tidy.log`) and `git diff --check` pass. Logs remain local
evidence pointers, not committed artifacts.
