---
schema: "implement-rfc.card.v1"
id: "BPR-2382"
title: "Settings › Storage & privacy › Clear local data is a compact table: one line per kind of data, and the header row's three-state box checks or unchecks them all"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"bf60b4ac0","origin":"owner rejection of the Clear local data look, attended review 374cb080088e516421011d3a88d707e0, 2026-11-24 (verbatim below)","reservation":"docs/issues/_reservations/BPR-2382--BPR-2382--82adbba22150.json","related_cards":["BPR-2377","BPR-2380","BPR-1602"],"acceptance":["BPR-2382:one-line","BPR-2382:header-check","BPR-2382:locks","BPR-2382:keyboard-and-names","BPR-2382:clear-path"],"owner_step":"attended look at the table (see Owner judgement)"}
acceptance: []
---

# BPR-2382: Clear local data is a compact table

## Why

The owner, rejecting [[BPR-2377]]'s Clear local data look in attended review
`374cb080088e516421011d3a88d707e0`, 2026-11-24: "The settings menu is too
expanded. Checkmark, label, and description should all fit a single line per
option. You can even make it a table if you want. Instead of a check all
button you can have the single check on the column header row to
check/uncheck all like standard UX."

### What existed

Each of the nine kinds was a checkbox with its name, then a wrapped
description of one to three lines under it. The list alone was about 520
points tall at the Settings window's own size and pushed **Clear…** off the
page. **Check all** was a button beside **Clear…**. A kind another checked one
included showed checked and locked, with the reason written ahead of its
description, which lengthened that row further.

## Change

Presentation only (`src/adapters/qt/src/profile_data_reset_views.cpp` and its
header). Core's categories, implication rule, plan and admission are
unchanged, as are the confirmation, the recovery dialog and the notices.

- **A table.** One grid inside a framed panel: a header row, then one row per
  kind in core's order. A row is its box, its name, one short line saying what
  it clears, and its size at the end of the line where one is known. The
  columns are titled "Data" and "What it clears". The list is about 275 points
  tall; the whole page fits the Settings window without scrolling.
- **One line each.** Every kind has a short line written for the row
  (`profile_data_reset_category_words::Brief`). The earlier whole account
  (`Line`) is the row's tooltip and the box's accessible description, so
  nothing the earlier look said is lost. A line too long for a narrow window
  ends in "…"; it never wraps, and it never widens the page.
- **The header row's box replaces Check all.** Unchecked with nothing
  checked, a dash with some, checked with all. A click checks every kind, or
  unchecks them all when all are checked. It is named "Check all" for screen
  readers and Settings search.
- **Locked rows.** A kind another checked kind includes is checked and
  disabled, the whole row dimmed; the reason leads its tooltip and its
  accessible description. It is locked whether or not the person had checked
  it too (so the header's box shows the same two rows locked as checking
  Settings and Agent sessions by hand); unchecking the kind that includes it
  gives the box its own choice back.
- **Rows are the target.** A click anywhere on a row is a click on its box;
  the header row is not a row, and a locked row takes no click.
- The lead above the table is one line at the window's own size: "Check what
  to clear, then Clear. It asks first, and nothing on GitHub changes." The
  confirmation already says that the profile's windows close and Home
  reopens.
- **Clear…**, its single confirmation and its request are as before.

`profile_reset_panel::select_all_button()` is now `select_all_box()`; the
reset course's click follows it.

## Acceptance

- **BPR-2382:one-line** `qt_profile_data_reset_views`
  (`test_one_line_per_category`), also at 125% and 200%
  (`qt_profile_data_reset_views_scale_125`, `_scale_200`, registered by this
  card). At the Settings window's own size, with a size shown on Agent
  workspaces: every row's box, name and line share one line, rows are one
  line apart, no name and no line is shortened, the size sits whole at the
  end of its row, and the group fits its page. Checking everything changes no
  row's height. At the narrowest Settings window the rows are still one line
  each, the table's height is unchanged, the page has no sideways range, and
  each line's whole account is in its tooltip. Pass.
- **BPR-2382:header-check** same executable
  (`test_the_group_on_storage_and_privacy`): no Check all button remains; the
  header box is three-state, unchecked with nothing checked, partially
  checked with some, checked with all (implied rows included); a click on a
  dash or an unchecked header checks every kind, a click on a checked header
  unchecks every kind. Pass.
- **BPR-2382:locks** same test: Settings locks Agent setup and Agent sessions
  locks Agent workspaces, checked and disabled with name and line dimmed; the
  reason leads the tooltip and the accessible description, which still say
  what the kind clears; the row's one line is unchanged; unchecking frees the
  row. A click on a locked row changes nothing. Pass.
- **BPR-2382:keyboard-and-names** same tests: every box and the header box
  take Tab focus; Space on a row's box checks it and Space on the header box
  checks or unchecks all; each box's accessible name is its kind (with its
  size when known) and its description the whole account; each name label is
  its box's buddy, so Settings search still finds "Agent sessions". A click
  on a row's name or line toggles its box; a click on a column title does
  nothing. Pass.
- **BPR-2382:clear-path** `application_profile_data_reset_public_path`: the
  composed application checks categories in the real Settings, presses
  **Clear…**, confirms once and reopens for each reset the course runs, and
  unchecks everything through the header box. Pass.

Evidence revision: bf60b4ac0. Commands, all passing on macOS (dev preset,
Debug):

```sh
ctest --preset dev -R '^(qt_profile_data_reset_views(_scale_[0-9]+)?|application_profile_data_reset_public_path|qt_settings_window(_scale_[0-9]+)?|test_network_isolation_manifest)$'
python3 _coding-standard/scripts/format_check.py <the four touched C++ files>
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths <the four touched C++ files>
```

Nine of nine tests passed; the format check and clang-tidy passed.
`qt_settings_window` and its three scales are included because they lay out
the page the group is on.

Renders (not committed), before and after: native macOS style kept off screen
(`BPR_NATIVE_LOOK=1`), Light and Dark, and offscreen at 100%, 125% and 200%.
The after set adds the group alone with nothing, some and everything checked,
at the window's own size and at its narrowest.

## Owner judgement

Pending: an attended look at the table, as the rejected look was. One
question: does Clear local data now read as one line per option with the
check-all in the header row? Accept or Reject, with any change in the note.

## Not done here

- Sizes for the other kinds. Only Agent workspaces has a measured size; the
  table shows a size wherever the host presents one.
- The wording of the size itself ("3 workspaces, 17.2 GB") is the host's and
  is unchanged.
