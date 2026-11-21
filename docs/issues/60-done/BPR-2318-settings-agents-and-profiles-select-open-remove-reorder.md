---
schema: "implement-rfc.card.v1"
id: "BPR-2318"
title: "Settings agents and profiles: a click selects, rows have grips and a trash, arguments are rows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect-ux","reservation":"docs/issues/_reservations/BPR-2318--BPR-2318--0f4595873d94.json","origin":"owner chat 2026-11-20 (\"Agents settings: a single click on an agent opens it's settings page but that should just select it. Double click and enter to open. There should be a visible trashcan on each row to remove the agent with a confirmation. Aagents should have drag handles to reorder how they appear. Arguments in the agent settings isn't good, needs to show more than one row, needs drag handles, needs scroll bar. Ps profiles needs the same drag handle.\")","related_cards":["BPR-1753","BPR-2051","BPR-0778","BPR-2198"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2318: Settings agents and profiles: a click selects, rows have grips and a trash, arguments are rows

## The owner's words (2026-11-20, verbatim)

"Agents settings: a single click on an agent opens it's settings page but
that should just select it. Double click and enter to open. There should be a
visible trashcan on each row to remove the agent with a confirmation. Aagents
should have drag handles to reorder how they appear. Arguments in the agent
settings isn't good, needs to show more than one row, needs drag handles,
needs scroll bar. Ps profiles needs the same drag handle."

## What was wrong

- Settings › Agents ([[BPR-2051]]'s roster) opened an agent's sheet on a
  single click, so a row could not be selected without opening it.
- Remove lived only in the row's menu and removed at once, without asking.
- The roster could be dragged, but nothing on a row said so.
- The agent sheet's Arguments was a plain-text box capped at 72 pixels: about
  one line showed, with no grips and no visible scroll bar.
- Settings › Profiles listed profiles in identity order and could not be
  reordered at all; the registry had no order to save.

## Decisions

- Every agent can be removed, Claude Code and Codex included: Add agent…
  offers a popular agent again while it is found, so no row needs a disabled
  trash.
- Removing an argument does not ask: the sheet has its own Cancel.
- The agent order already lived in the core (`move_agent_entry`), was saved
  as the order of the agents list and is the order the footer's picker and
  the Agent tab use; it is unchanged. Only the presentation is new.
- The profile order is a registry fact, so it is saved with the registry. A
  registry is written as version 3 (a new `profile_registry_order` table)
  only when profiles are first reordered: one never reordered stays version 2,
  which builds before this card still read.

## Acceptance

- AC1. In Settings › Agents a single click selects an agent and opens
  nothing; a double-click, Return or Enter opens its sheet.
- AC2. Every agent row shows a trash at its right, named "Remove <agent>".
  It, Remove in the row's menu and Delete ask first ("Remove <agent>?",
  Cancel and Remove agent); only Remove agent removes it.
- AC3. Every agent row shows a grip at its left, named "Reorder <agent>". A
  drag moves the agent; Alt+Up and Alt+Down move the selected one. The order
  is the core's saved order, used by the footer picker and Agent tab.
- AC4. The agent sheet's Arguments shows one argument per row, five rows and
  a scroll bar past them, with a grip and a trash per row, Add argument,
  Alt+Up and Alt+Down, and Delete. Arguments are kept trimmed, in order, and
  an empty row is none.
- AC5. Every Settings › Profiles row has a grip, named "Reorder <profile>";
  a drag or Alt+Up and Alt+Down ask the host to move the profile, the list,
  the window's Profiles menu and the desktop menu bar follow the saved order,
  and nothing moves while a profile is being named.
- AC6. A registry saved before this card loads with no order and lists
  profiles by identity; the order survives a restart; a damaged order is
  refused.

## What changed

- **Qt, shared** (`src/list_row_handles.*`). A grip at every row's left of a
  `QListWidget` and, when its rows can be removed, a trash (`quiet_tool_button`
  with the comments' trash glyph, in the selection's ink on a selected row) at
  its right. The grip lets the mouse through, so a press on it starts the
  list's own drag; the hand cursor shows over it. Each handle is named for its
  row ("Reorder Work Claude", "Remove Work Claude"); the grip's description
  says how to move from the keyboard. With the list focused, Alt+Up and
  Alt+Down move the current row and Delete or Backspace removes it. The
  handles follow rows added, removed, moved, renamed or scrolled, and are
  placed against the viewport's edge, so a resize never leaves them outside.
- **Settings › Agents** (`agent_setup_settings.cpp`). A click selects; a
  double-click, Return or Enter opens the sheet. The roster's chevron is gone
  (it said a click opens); the ring and words move right of the grip. Trash,
  Remove in the row's menu and Delete open a `destructive_question`
  ("Remove Work Claude?", Cancel, Remove agent). Moves still go through the
  existing `Move` dispatch to `core::move_agent_entry`.
- **Agent sheet** (`argument_list_editor`). One argument per row in the code
  font, five rows then a scroll bar, a grip and a trash per row, Add argument
  (adds a row and edits it), double-click or the edit key to edit, Alt+Up and
  Alt+Down, Delete. An emptied or untyped row is dropped once its editor
  closes. `entry()` takes the rows trimmed and in order, as the text box took
  its lines.
- **Profiles, application.** `profile_registry_snapshot::Order` (empty: identity
  order), `listed_profile_order`, `moved_profile_order`,
  `profile_registry_host::move` and `move_bpr_profile_intent`.
  `valid()` refuses an order that is not every profile once; a profile created
  after an order is saved goes last. `profile_management_host` lists every
  projection in the accepted order and acknowledges a saved move as an
  accepted edit; a refused move leaves the list as it was.
- **Profiles, SQLite** (`profile_registry_store.cpp`). The first save with an
  order adds `profile_registry_order` and stamps `user_version` 3 inside the
  same write transaction; later saves replace its rows. Version 1 and 2 files
  load as before with no order. The schema check allows one more table at
  version 3 and refuses an extra one.
- **Settings › Profiles.** Grips, InternalMove drag and the keyboard moves
  dispatch the intent; the row waits for the host's projection, which
  `adopt` already reorders by identity. Drag is off while a profile is being
  named.
- **Docs.** `docs/development/settings.md` (Profiles, Agents, Arguments) and
  `docs/development/settings-authority-inventory.md` (profile order row).

## Evidence

Candidate `3224b9b95` (rebased on main `a4f8a92a2`), dev preset (Debug),
macOS, offscreen Qt:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `qt_agent_setup_view` › `test_settings_rows_select_open_remove_reorder`: a press and release on row 1's viewport selects it and the editor is not called; press, release, double-click, release opens "Work Codex"; Return and keypad Enter open it again | Passed |
| AC2 | Same course: three rows each have a visible trash inside the row's right half with an icon; "Remove Harness" name and tooltip; the trash selects its row and opens "Remove Harness?" with nothing sent; Cancel sends nothing; confirming row 0's trash sends `remove:1`; Delete opens the question. `test_settings_rows`: Remove asks before `remove:2`. `application_agent_chat_setup_list_contract`: Settings Remove, question accepted, agent gone from Settings and setup | Passed |
| AC3 | Same course: each row's grip is visible at the row's left edge, named "Reorder Work Claude", described with Alt+Up, mouse-transparent; Alt+Down/Alt+Up send `move:2:1`, `move:2:-1`, the first row goes no higher; a model drop two places down sends `move:1:1` twice; a re-presentation restores the host's order with the handles matched to the rows. Order persistence and use: unchanged `core::move_agent_entry` (`core_agent_entry`) | Passed |
| AC4 | `qt_agent_setup_view` › `test_arguments_editor` and `test_settings_rows`: five rows fit and three need no scroll bar; twelve show a scroll bar and the last row's grip follows a scroll; grips and trash named per argument; Alt+Up moves and keeps the selection; a drop reorders; a trash removes; Add argument edits a new last row and keeps `--model sonnet` as written; Escape on an empty new row drops it; `" --profile work "` and `""` give `--profile work` and nothing | Passed |
| AC5 | `qt_profiles_settings_panel` › `test_profiles_are_reordered_by_grip_and_keyboard`: a visible grip per row named "Reorder Work & Research", InternalMove; Alt+Up dispatches `move_bpr_profile_intent{3,-1}` and the row waits; the top row goes no higher; a drop two down dispatches `{1,2}`; adopting the host's order reorders rows and grips; no drag or keyboard move while naming, back after Cancel. Menus: the same course's window and desktop Profiles menu cases, which reorder actions to the projection | Passed |
| AC5, AC6 | `application_profile_management_host` › `test_profiles_are_listed_in_the_saved_order`: identity order with none saved; a move is not shown until saved, then lists Home first as an accepted `move` edit with no runtime command; a rename keeps it; a move off the top is refused (`unknown_profile`) with the order kept; a restarted host lists Home first | Passed |
| AC6 | `application_profile_registry` › `test_profile_order`: `moved_profile_order` up, down, by two, and its refusals; no order is added by creates; a move is accepted only after the save; created-after-order goes last; duplicate or missing order refused by `valid()` | Passed |
| AC6 | `sqlite_profile_registry_storage` › `test_profile_order`: a registry with two created profiles stays `user_version` 2 with no order; the first order is written, read back after reopening and stamps 3; a later save replaces it; a gap at position 0, an unknown profile, a missing profile and an extra table are each refused | Passed |
| Regression | `application_profile_startup_admission`, `application_profile_storage_authority`, `application_profile_startup_public_path`, `application_profile_management_public_path`, `qt_settings_window` at 100/125/150/200 | Passed |
| Boundaries | `architecture_boundaries` | Passed |
| Standard | `_coding-standard/scripts/format.py` (clang-format 22) on every changed C++ file; `tidy.py` on the eight changed product translation units: `clang-tidy passed` | Passed |
| Docs | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

`coding-format-check` over the whole tree reports one file,
`tests/end_to_end/follow_up_public_path.cpp`, which this card does not touch
(last changed by `58eae7f05`). `qt_profiles_settings_panel` failed on main
before this card: [[BPR-2308]] removed the desktop File menu's Open Project…
and its expectation still counted five items; the course now expects four and
runs to its end.

Not verified: the native macOS look (Cocoa, `BPR_NATIVE_LOOK=1`), VoiceOver
reading the grips and trash, a real mouse drag (the courses drop through the
model's `moveRow`, which is the drop's own path), and Windows and Linux runs.
Offscreen Fusion renders of the roster and the sheet were checked for layout
only. No attended look was asked for in this round.
