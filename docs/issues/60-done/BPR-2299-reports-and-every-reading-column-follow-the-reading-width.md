---
schema: "implement-rfc.card.v1"
id: "BPR-2299"
title: "Reports, Discuss and Threads follow one Reading width, with per-surface widths under Advanced"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2299--BPR-2299--ad4dc7e277f7.json","origin":"owner requests 2026-11-19, relayed verbatim by the coordinator","acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"],"not_done":"Profile Settings (.bprsettings) export and import: the file has no section for any Review setting, so it carries neither the Reading width nor the surfaces' widths; see Open question"}
---

# Reports, Discuss and Threads follow one Reading width, with per-surface widths under Advanced

## What the owner asked

In order, 2026-11-19:

- "The current report width feels too narrow by default and I think it
  should also become a setting. Or perhaps just drive it off of the existing
  reading width setting."
- "I think we can just use reading with for everything. Although I would add
  a custom option to it."
- "Don't bother carrying over the discuss column choice. Just scrub all
  references to that setting. Except for of course, don't fail on loading
  that setting from an older settings."
- "Maybe instead of custom we have an advanced button and advanced can set
  the width per feature including to arbitrary custom values."
- "Right even for custom we should put sensible minimums." Then: "I mean we
  probably just use narrow as the minimum".

## Before

| Surface | Width | Setting |
| --- | --- | --- |
| Report tab's page | 572 px, fixed (`agent_report_view.cpp`, `column_width`) | none |
| Discuss's centred list | 680 / **820** / 1100 / Full | Discuss column ([[BPR-2171]]), its own store |
| Threads' rows, and the Discuss panel | 960 / 1600 / **2560** / Full | Reading width ([[BPR-1584]]) |

The Reading width's presets were wider than any of the columns people read,
so on an ordinary screen it limited nothing; the column actually seen in
Discuss was the Discuss column's 820.

## After

One **Reading width** (Settings › Review › Reading) sets every reading
column: Threads' rows (from the page's left), Discuss's centred list and a
report tab's centred page. Presets: Narrow **720**, Medium **900**, **Wide
1100 (default)**, Full width. Ordinals are unchanged, so a profile on the old
default reads the new one.

**Advanced…** beside it opens a dialog (`reading_width_dialog`) with one row
each for Threads, Discuss and Reports: Use Reading width (the default,
naming the width it is), Narrow, Medium, Wide, Full width or Custom width
(logical pixels, 720 to 3840). "Use Reading width for all" and the Reading
group's Reset take every surface back. Changes apply at once, to the report,
list and rows already open.

Core (`workbench_interaction.hpp`): `reading_surface`, `reading_override`,
`reading_override_of` (holds a width to 720..3840), `reading_surface_limit`
(a surface's own width, else the Reading width's), and the stored form
(`stored_reading_override` / `read_reading_override`: 0 follows, -1 whole
page, a too-small width read back as 720). The minimum is Narrow's width,
asserted at compile time.

Storage: workbench settings schema 36 adds `reading_threads`,
`reading_discuss` and `reading_reports` (CHECK -1..3840); a version-35 row
migrates following the Reading width everywhere. The Discuss column is gone
from core, the application host, the window, Settings and the store;
`discuss.sqlite3` is created without it, and a store that still has it loads
with its marks and the column dropped unread.

The report's code boxes and suggestion diffs take the wider column and still
scroll sideways inside their box; the page itself never scrolls sideways.
Uncapped (Full width), the report column takes the whole pane: the side
stretches that centre it take nothing.

## Acceptance

- **AC1.** A report's page follows the reports' reading width live, centred,
  never at the old 572; its code box and suggestion scroll inside their box
  and the page never scrolls sideways, at every preset, a custom width and
  Full width. `qt_agent_report_view` (`check_reading_width`).
- **AC2.** Threads, Discuss and reports follow the Reading width live from
  Settings; the code comparison is unchanged at every preset.
  `qt_thread_moves_scale_100/125/200`
  (`test_reading_width_is_never_the_code`), `qt_conversation_finder_scale_*`.
- **AC3.** Advanced: a custom width for one surface, a width typed below 720
  held to 720, Full width and Narrow on others, a preset leaving own widths
  alone, Use Reading width for all; the dialog states the 720 minimum and its
  box holds to it. Same test.
- **AC4.** Core: presets, resolution, bounds, validity, stored form, a
  too-small stored width clamped. `core_workbench_interaction`
  (`test_the_reading_width`, `test_reading_surface_overrides`).
- **AC5.** Store: version 35 migrates, own widths and the whole page round
  trip, a 300 written past the app loads as 720 and the record is valid.
  `sqlite_workbench_settings_storage`
  (`test_reading_surfaces_from_thirty_five`); every older-version fixture
  still migrates.
- **AC6.** The Discuss column is gone and an older store holding it loads.
  `sqlite_discuss_preferences_store`, `core_discuss_preferences`,
  `application_discuss_preferences_host`; Settings shows no Discuss column
  row (`test_reading_width_is_never_the_code`).
- **AC7.** Discuss's list in the composed application is the Reading width's
  1100, centred, at 1280, 1728 and 2560 wide, with Only this file.
  `application_discuss_busy_public_path`.

## Evidence

macOS arm64, dev (Debug), offscreen.

| Criterion | Check | Result |
| --- | --- | --- |
| AC1 | `qt_agent_report_view` | Passed; at 3200 px the column is 1100 (Wide), then live 720, 900, 1480, the whole room (Full) and 1100 |
| AC2, AC3, AC6 | `qt_thread_moves_scale_100`, `_125`, `_200` | 3 of 3 passed |
| AC2 | `qt_conversation_finder_scale_100`, `_125`, `_200` | 3 of 3 passed |
| AC4 | `core_workbench_interaction`, `core_discuss_preferences` | Passed |
| AC5 | `sqlite_workbench_settings_storage` | Passed |
| AC6 | `sqlite_discuss_preferences_store`, `application_discuss_preferences_host` | Passed |
| AC7 | `application_discuss_busy_public_path` | Passed (`DISCUSS_COLUMN 90+1100 of 1262`) |
| Regression | `qt_conversation_cards_view`, `qt_github_conversation_panel`, `qt_conversation_preview`, `application_setup_destination_public_path` | Passed |

Format check passed on the 31 touched C++ files. clang-tidy on the touched
translation units reports nothing in the changed lines; what it reports in
them is on main unchanged (`agent_report_view.cpp` 713, 885, 1067;
`discuss_busy_public_path.cpp`'s `scroll_to`; `local_draft_workflow_area.hpp`).
The `frontend-design` skill was loaded for this Qt surface.

## Open question

The owner's later relay asked that the widths travel in Profile Settings
(`.bprsettings`, version 3). That file has six sections (Agents, Prompts,
Workflows, Identity, Repositories, Theme) and no Review settings at all: it
carried neither the Reading width nor the Discuss column before this card.
Carrying them means a new section and its import rules, which is the owner's
call; nothing here changes the file format.
