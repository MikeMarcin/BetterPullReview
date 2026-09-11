---
schema: "implement-rfc.card.v1"
id: "BPR-0721"
title: "A setting hides the drawer-side controls, and a collapsed drawer shows only its rail"
status: "Parked"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0018"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
parked_on: "human-review"
acceptance: [{"criterion":"qt_composed_github_scale_{100,125,200}: with the setting off, a collapsed drawer occupies zero width in the splitter and no Hide button is in the drawer; with it on, the rail is exactly the 18-pixel column with its label and nothing above it; each state pinned in a rendered crop","outcome":"pass","revision":"d6106184","evidence":{"kind":"command","detail":"build/dev ctest qt_composed_github_scale_100/125/200 pass (scratch ctest-qt-1.log, c7.log, ctest-focused-1.log): test_agent_surfaces_are_placed_in_the_conversations_drawer asserts !drawer->isVisible(), splitter sizes()[3]==0 and controls_shown()==false with the default record, then after publishing DrawerControls=shown asserts rail visible, width==18, the heading label hidden and the Hide button hidden; test_a_pull_request_without_a_comparison_still_lists_its_files asserts no Hide button by default and the strip toggle enabled, then the named Hide button once shown. Crops composed-github-{100,125,200}-drawer-controls-hidden.png and -drawer-controls-rail.png in build/dev/tests/adapters_qt; the rail crop was inspected at 100 and showed the rail at the window's right edge with nothing above it (a first cut left the rejoined panel granted 375 px by the splitter, fixed by re-applying central_sizes). qt_workbench_window_scale_100/125/200 pass with test_hidden_drawer_controls_leave_no_column_and_no_hide_button (zero splitter width, no handle left, Hide button only when shown, heading visible when expanded) and the Reviews & queue row test extended for the Drawers group."}},{"criterion":"The settings store round-trips the flag through the new schema version; the migration test upgrades the previous version's database and keeps every existing column","outcome":"pass","revision":"d6106184","evidence":{"kind":"command","detail":"Schema version 6 (the store was already 5, so the card's 'version 5' is 6): drawer_controls INTEGER NOT NULL DEFAULT 0 CHECK BETWEEN 0 AND 1. sqlite_workbench_settings_storage passes (scratch ctest-focused-1.log) with test_a_version_five_store_migrates_and_gains_the_drawer_controls: a literal version-5 file with every profile value non-default and one window row opens, every value survives, DrawerControls is hidden after migration, shown round-trips, stamp and meta equal the constant, the old table is gone, and a value of 2 past the check is refused with schema_mismatch; the version 1-4 migration tests and the interrupted-migration test (now built from the constant, and run against a version-5 file too) pass. application_workbench_host and application_workbench_preferences_consumers pass."}},{"criterion":"application_composition_contract and the workbench composition contract (when BPR-0718 has made it green) pass with digests repinned; check_boundaries, strict tidy, clang-format --Werror and the documentation gate are clean","outcome":"pass","revision":"d6106184","evidence":{"kind":"command","detail":"application_composition_contract passes (144.3 s, scratch task output); no digest moved because src/app/src/github_application.cpp is untouched (its collapsed-drawer assertions use width <= 18, which a hidden panel satisfies). application_workbench_composition_contract fails at head with PUBLIC_PATH_STAGE:2 presentation=0, the pre-existing BPR-0718 failure, so it is not evidence either way. cmake --target check_boundaries: both boundary scripts verified. tidy.py --warnings-as-errors on workbench_drawer.cpp, workbench_window.cpp, workbench_window_tests.cpp, composed_github_tests.cpp, workbench_settings_database.cpp and workbench_settings_storage_tests.cpp: exit 0 (scratch tidy1.log, tidy2.log). format_check.py on every touched file: exit 0. scripts/verify_documentation.py --root .: DOCUMENTATION_OK."}},{"criterion":"The owner accepts both states on the signed bundle in the next attended session","outcome":"pending","revision":"d6106184","evidence":{"kind":"freeform","detail":"Asked in the v004 packet on a bundle rebuilt from this head: by default the drawers show no rail and no Hide button and the strip toggles open and collapse them; Settings > Reviews & queue > Drawers > Drawer controls: Shown brings the rails and Hide buttons back with nothing above a rail."}}]
---

# A setting hides the drawer-side controls, and a collapsed drawer shows only its rail

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
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
[BPR-0675](BPR-0675-a-drawer-cannot-be-collapsed-by-anyone-who-has-not-read-the-code.md)),
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

## Landed, 2026-09-10

Facts the card did not have: the store was already schema version 5
(the per-window table), so the flag is version 6's `drawer_controls`
column; the drawer shortcuts are Alt+Shift+1/2/3, not ⌘1 to ⌘4; and the
Reviews & queue page is `build_behaviour_editor` in `workbench_window.cpp`,
which the Settings window adopts, not `settings_window.cpp`.

- The defect: `workbench_drawer_panel::present()` sets `Heading` and
  `Collapse` by name, since the heading row is a nested layout the column
  walk never reached. A collapsed drawer with the controls shown is its
  18-pixel rail and nothing above it.
- The setting: `review_behaviour_settings::DrawerControls`
  (`review_drawer_controls::hidden` by default, `shown`), a row "Drawer
  controls" (Hidden / Shown) in a third group "Drawers" with its own Reset
  on Reviews & queue. It rides the existing behaviour record through the
  host unchanged; the window applies it in `apply_drawer_layout`. Hidden, a
  collapsed panel leaves the splitter (`hide()`), so its handle goes with
  it and `central_sizes` counts neither; an expanded panel has no Hide
  button. A panel that rejoins the splitter is sized again explicitly, with
  the design widths when the host has published none, because the splitter
  otherwise grants it the room it last had.
- Tests that pin the rail publish `DrawerControls = shown` first;
  `test_hidden_drawer_controls_leave_no_column_and_no_hide_button` and the
  composed conversations-drawer course pin both states, the latter with
  right-edge crops `composed-github-{100,125,200}-drawer-controls-hidden.png`
  and `-drawer-controls-rail.png` in the build tree.
- RFC-BPR-0028: the Modalities and Files-drawer paragraphs, the Settings
  page list and AC-03 now say the rail is the collapsed presentation while
  the drawer controls are shown, and no column while they are hidden.
- Not amended: `application_uiforetw_selected_diff_contract` asserts
  `width <= 18` for the collapsed drawers, which a hidden panel satisfies,
  so its text needed no change; it could not be rerun because it fails at
  startup on head independently
  ([BPR-0723](../20-ready/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)).
  `application_workbench_composition_contract` stays red at head
  (`PUBLIC_PATH_STAGE:2`, presentation=0), the failure
  [BPR-0718](../20-ready/BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
  records, so it is not this card's evidence either way.
- Attended: BPR-0675's candidate criterion ("each drawer's own Hide and the
  strip toggle both collapse it") now needs the setting on; the v004
  packet says so.
