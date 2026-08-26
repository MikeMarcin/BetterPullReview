---
id: BPR-0457
title: Add the View menu and make line numbers a toggled, persisted setting
status: Ready
rfc: RFC-BPR-0005
depends_on: [BPR-0455]
---

# Add the View menu and make line numbers a toggled, persisted setting

The owner asked for two things in one sentence:

> there should be line numbers on by default (toggle off with view menu
> option)

[[BPR-0455]] delivered the first half — numbers, on by default — because that
is what unblocked the [[BPR-0414]] review. This card owns the second half, and
it is split off because it is a cross-layer slice rather than a switch.

## Why the toggle could not ride along

There is no cheap version. `src/adapters/qt/AGENTS.md` requires Qt to
"translate input into semantic intents; do not perform host effects", and
RFC-BPR-0005 requires that "`QAction` enabled/checked state comes from one
command projection". A Qt-local boolean on the pane would satisfy the owner's
sentence and violate both, so the honest implementation follows the
drawer-collapse path end to end:

- `core::workbench_interaction_state` gains the visibility field
  (`src/core/include/bpr/core/workbench_interaction.hpp:260`; note the
  `sizeof <= 128` static assert it must stay inside);
- a new intent joins the `workbench_interaction_intent` variant beside
  `toggle_workbench_drawer_intent` (line 148), with its transition and its
  published delta on `workbench_interaction_output`;
- `application::workbench_layout_settings`
  (`workbench_runtime_ports.hpp:395`) gains the durable field;
- `src/adapters/sqlite/src/workbench_settings_database.cpp` gains a column on
  the STRICT `workbench_layout` table, which is a forward-only migration
  against existing rows rather than an additive edit to a fresh schema;
- `workbench_window` gains the action and applies the projection, exactly as
  `dispatch_drawer_toggle` and `apply_drawer_layout` already do.

## The menu is a real decision, not a detail

**This application has no menu bar.** `workbench_window` creates exactly one
`QMenu` — `ReviewControlsMenu` at `src/adapters/qt/src/workbench_window.cpp:474`
— opened from a toolbar tool button. `menuBar()` is never called.

So "view menu option" needs an answer, and the two candidates are not equal:

- **A real `QMenuBar` with a View menu.** Matches the owner's words. On macOS it
  merges into the system menu bar and costs no window space, which suits the
  same verdict's request for more room for source. On Windows and Linux it
  takes a row from the window, which works against that request.
- **A checkable action in the existing toolbar surface**, following
  `ReviewDetailsAction` (`workbench_window.cpp:433`, checkable, `Alt+Shift+3`).
  Cheap and consistent with what is there, but it is not what was asked for and
  a reviewer looking for a View menu will not find it.

Do not pick silently. If the menu bar is chosen, note that
`test_compact_icon_row_preserves_exact_order_and_edges`
(`tests/adapters_qt/workbench_window_tests.cpp:539`) asserts the toolbar's exact
action order and both its edges, and `test_focus_order_is_declared_named_and_walkable`
(line 3091) asserts the declared focus order — a new action must be placed
deliberately with respect to both.

## Required final state

- Line numbers can be turned off and back on from where the owner expects.
- The action's checked state comes from a projection, not from widget state.
- The setting survives a restart.
- Turning it off returns the gutter's width to the code, and every geometry
  surface [[BPR-0455]] proved unmoved stays unmoved in both states.

## Verification

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'workbench|diff_workbench|sqlite_workbench|core_workbench|composed_github'
```

A rendered check in both states is required, plus a restart-persistence check
through the real settings database rather than a fake port alone.
