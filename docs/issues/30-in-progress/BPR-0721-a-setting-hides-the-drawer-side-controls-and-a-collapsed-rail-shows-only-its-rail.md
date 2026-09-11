---
schema: "implement-rfc.card.v1"
id: "BPR-0721"
title: "A setting hides the drawer-side controls, and a collapsed drawer shows only its rail"
status: "In Progress"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0018"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
---

# A setting hides the drawer-side controls, and a collapsed drawer shows only its rail

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"The drawers on the left are taking up way too much window space when
collapsed. I'm thinking the top buttons in the title bar area are
sufficient and we don't need the drawer columns or even the hide byttons on
the drawers themselves. Probably on the right side as well. I dont think I
want to fully remove them just yet though, can we make a setting to
enable/disable the drawer controls (leaving just the title bar buttons when
setting is disabled)?"

Two things follow, one a defect and one owner direction; they touch the same
widgets, so they are one card.

## The defect: a collapsed drawer shows more than its rail

The owner's third screenshot, with Files and Repositories collapsed, shows
each 18-pixel rail with a small "…" button above it in the drawer's header
row. `workbench_drawer_panel::adopt()`
(`src/adapters/qt/src/workbench_drawer.cpp:517-524`) hides the column's
direct child widgets when collapsing, but `Heading` and the `Collapse`
("Hide") button live in a nested `headingRow` layout (from 86375805,
[BPR-0675](../40-parked/BPR-0675-a-drawer-cannot-be-collapsed-by-anyone-who-has-not-read-the-code.md)),
so the row stays visible in the rail and the Hide button is squeezed to
"…". A collapsed drawer shows its rail and nothing else, and the composed
test that pins the rail asserts the heading row is hidden. This part is not
optional and does not wait on the setting.

## The direction: a setting that removes the drawer-side controls

A workbench setting, off by default until the owner says otherwise, "Show
drawer controls". When it is off, a collapsed drawer leaves no column at
all (no rail, no label, no count) and an expanded drawer carries no Hide
button; the strip's three drawer toggles and ⌘1 to ⌘4 remain the only way
to open and collapse a drawer, and the drawer state itself is untouched
because the core still decides it (`set_workbench_drawer_intent`,
`toggle_workbench_drawer_intent`; `src/core/src/workbench_interaction.cpp:369-384`).
When it is on, today's rails and Hide buttons return. The setting is
presentation only: no new authority in Qt.

RFC-BPR-0028 AC-03 and the rails paragraph describe the 18-pixel rails as the
collapsed state; when this card lands the RFC is amended to say the rail is
the collapsed presentation while drawer controls are shown, and that the
setting exists. The owner has not asked to remove the rails outright.

## Scope and owned paths

- `src/adapters/qt/src/workbench_drawer.cpp` and
  `src/adapters/qt/include/bpr/qt/workbench_drawer.hpp` (rail, heading row,
  Hide button, collapsed width); `src/adapters/qt/src/workbench_window.cpp`
  (the drawer columns and splitter handles when no rail is shown; the strip
  toggles at lines 392-411 are unchanged); the Settings window page that
  carries workbench options (`src/adapters/qt/src/settings_window.cpp`,
  RFC-BPR-0018 layout); the workbench settings record and its SQLite store
  (`src/adapters/sqlite/src/workbench_settings_database.cpp`, schema version
  4 today, so a version 5 with the new column and its migration test); the
  application's settings projection that carries the flag to the window.
- Not in scope: removing the rails, changing the strip, Discuss's widening
  (BPR-0649), the unread count on the rail (BPR-0648).

## Acceptance

- `qt_composed_github_scale_{100,125,200}`: with the setting off, a
  collapsed drawer occupies zero width in the splitter and no Hide button is
  in the drawer; with it on, the rail is exactly the 18-pixel column with its
  label and nothing above it. Each state is pinned in a rendered crop.
- The settings store round-trips the flag through schema version 5; the
  migration test upgrades a version-4 database and keeps every existing
  column.
- `application_composition_contract` and the workbench composition contract
  (when BPR-0718 has made it green) pass with digests repinned;
  `check_boundaries`, strict tidy, `clang-format --Werror` and the
  documentation gate are clean.
- The owner accepts both states on the signed bundle in the next attended
  session.
