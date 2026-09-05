---
schema: "implement-rfc.card.v1"
id: "BPR-0457"
title: "Add the View menu and make line numbers a toggled, persisted setting"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0455"]
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

## Done — 2026-08-26

### The menu was the owner's decision, and it was asked

The card said not to pick silently, so the choice was put to the owner on
2026-08-26 with both options and their costs. Verbatim answer: **a real
`QMenuBar` with a View menu** — the option that matches the original words,
and on macOS the one that merges into the system bar and costs the window no
height. On Windows and Linux it takes a row. That is the price of the same
answer being true everywhere, and it was accepted knowingly rather than
discovered afterwards.

The window had no menu bar at all before this; `menuBar()` was never called.
It has one now, holding one menu.

### The gate is the drawer's gate, and the entry is greyed before a PR opens

The first implementation left the command always enabled, on the reasoning that
numbering the source is a reader's preference rather than an operation on an
open review. `tests/core/workbench_interaction_tests.cpp` refused it:

```
FAILED: an inactive workbench disables every workbench command
```

That invariant is deliberate and is asserted over every entry of the command
array. Rather than carve out the one exception, the command follows the drawer
rule — `surface_rejection`, the same predicate, renamed from `drawer_rejection`
now that two things use it.

**The consequence, stated here rather than left for a screenshot: View > Line
numbers is disabled until a pull request is open.** An owner who launches the
application and opens the View menu first will find the entry greyed. That is
consistent with every other workbench command and it is what the invariant
requires, but it is not what someone checking "did they add the view menu
option?" expects to see. It is recorded in the workbench specification too.
Ungating it honestly would mean moving line numbers out of `workbench_command_id`
altogether, which is a larger change than this card.

### The migration is a rebuild because `ADD COLUMN` cannot run here

Schema version one becomes two — the store's first real migration, against a
table with rows in it. The header used to say inventing a second version "would
exercise nothing"; that is no longer true.

It is not the one-line `ALTER TABLE ... ADD COLUMN` it should be. This
connection prepares with `SQLITE_PREPARE_NO_VTAB`, and adding a constrained
column makes SQLite verify the altered table through `pragma_quick_check` — a
pragma *virtual table*, which that flag refuses. Reproduced directly rather than
assumed: preparing the identical statement with no flags returns 0, with
`SQLITE_PREPARE_PERSISTENT` returns 0, and with `SQLITE_PREPARE_NO_VTAB`
returns 1 and `no such table: pragma_quick_check`. `ALTER TABLE ... RENAME TO`
prepares cleanly under the same flag, which is what makes the rebuild possible.
The failure was slow to find because `sqlite3_errmsg` was still reporting a
stale message from an unrelated earlier probe on the same connection.

So the migration is SQLite's table rebuild: create, `INSERT ... SELECT`, drop,
rename. It uses only statements this connection already runs, and it has the
better property anyway — both the fresh table and the migrated one come from
one `layout_table(name)` definition, so they cannot drift into merely
resembling each other. Note for whoever copies this: the canonical recipe says
to disable foreign keys first. This schema has none, so it does not matter
here, and it will matter in a store that does.

### What the tests could not see, and what an independent review found

An independent reviewer was given the diff, the claims, and the commands, and
returned one blocking finding that was correct and that the author's own five
negative mutations had all missed.

Every mutation tested a layer in isolation. None crossed the seam between the
window and the comparison surface — the three lines in `apply_line_numbers`
that carry the core's decision to the panes. Deleting them was reproduced
directly: **16 of 16 tests still passed**, the documentation gate still said
`DOCUMENTATION_OK`, and the format gate still passed, while the menu entry
toggled a check mark, dispatched an intent, mutated core state, wrote a durable
column, and the panes went on drawing numbers forever.

`test_the_view_menu_reaches_the_composed_source_panes` now closes it, through
the composed window rather than a bare surface, and it fails on exactly that
mutation. The same test covers the geometry claim in the numbers-**off** state,
which nothing previously reached: ribbon endpoints and the detail pane's width
are asserted unchanged across the round trip, because hiding the gutter moves
the text viewport horizontally and a stale measured band is the BPR-0208 class
of defect the predecessor card warned about.

The reviewer also caught two doc comments that had been orphaned onto the wrong
functions — including the privacy note about the schema naming no review
content, which had drifted onto the DDL helper — a stranded paragraph in the
workbench specification, the missing `Alt+Shift+5` row in the normative command
map, a missing accessible description on the menu bar, and the absence of a
claim-matrix row. All are fixed.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the entry exists in a View menu, is checkable, and emits one intent | `ctest --preset dev -R 'qt_workbench_window'` | the menu bar, the View menu, the action's presence in it, and exactly one `toggle_workbench_line_numbers_intent` per trigger | pass, 3/3 scales |
| the checked mark comes from the projection, not the widget | same | Qt flips a checkable action's mark before any handler runs, so the mark is re-asserted from `LineNumberState` and still reads `checked` after a trigger the core has not answered | pass |
| the enabled state comes from the command projection | same | overriding the seventh entry to `refused` disables the entry | pass |
| the decision reaches the composed panes | same | a published hidden state collapses both gutters in the real window's own comparison surface | pass |
| hiding returns the width to the code | `ctest --preset dev -R 'qt_diff_workbench'` and the composed test above | the text moves left by exactly the gutter's width, measured as a delta because the pane frame also contributes | pass, both surfaces |
| the geometry BPR-0455 proved unmoved stays unmoved, in both states | composed test, plus `ctest --preset dev -R 'qt_appearance_profile\|qt_syntax_pane_composition\|qt_epoch_comparison\|qt_github_thread_map\|qt_github_conversation_panel\|qt_diff_link_map\|qt_comparison_scroll_controller'` | every ribbon endpoint and the detail pane's width identical with numbers on, off, and on again; the geometry suites this card's own selector omits | pass, plus 15/15 |
| the setting survives a restart | `ctest --preset dev -R 'sqlite_workbench\|application_workbench_host'` | the real database closes and reopens with the value; the host replays it into the core on restore | pass |
| a version-one database keeps its row | `ctest --preset dev -R 'sqlite_workbench'` | all four stored values survive, the added column defaults to shown, and both the stamp and the meta row read two | pass |
| the core refuses the gesture with no workbench | `ctest --preset core-only -R 'core_workbench'` | an inactive workbench rejects it and retains nothing | pass |
| the whole selector | `ctest --preset dev -R 'workbench\|diff_workbench\|sqlite_workbench\|core_workbench\|composed_github'` | — | 16/16 |

### The tests fail when the claim is false

Six mutations, each reverted, each failing on its intended assertion.

| mutation | result |
|---|---|
| the hidden gutter still reserves its width | **fails**, "the gutter reserves nothing" |
| the widget decides its own check mark | **fails**, "triggering changes no state locally" |
| the host never adopts the field into the durable record | **fails**, "the durable record kept the source-pane numbering" |
| the rebuild copies no rows | **fails**, "every value the version-one row held survived the migration" |
| the rebuild leaves the meta row saying version one | **fails**, the store will not open |
| the window never tells the surface | **fails**, "a published hidden state reaches the composed panes" — and passed 16/16 before the composed test existed |

### Not proven here

The macOS system-menu-bar merge is **not** covered by automated evidence, and
the claim is not made on their strength. Every Qt suite runs under
`QT_QPA_PLATFORM=offscreen`, where a `QMenuBar` is an ordinary in-window widget
that takes a row — so the offscreen suites demonstrate the Windows and Linux
presentation, and actively contradict the no-height property that motivated the
owner's choice. `src/adapters/qt/AGENTS.md` already governs this: offscreen
renders are "deterministic layout evidence only; they do not verify system
appearance integration."

What would settle it is a target-native run of the real application under
Cocoa, checking that the View menu appears in the system bar and that
`Alt+Shift+5` fires there. That is a rendered, owner-facing check rather than
an unattended one, and it belongs with the RFC-BPR-0005 native visual
acceptance pass in [[BPR-0100]] rather than being invented here.
