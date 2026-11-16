---
schema: "implement-rfc.card.v1"
id: "BPR-2209"
title: "Settings › Prompts: Add to names the surface of each group"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win","mac","linux"]
metadata: {"found_by":"owner, 2026-11-15, looking at the prompt library row's right-click menu","reservation":"docs/issues/_reservations/BPR-2209--BPR-2209--8a0d7047c979.json","related_cards":["BPR-2179"],"lane":"claude/v0-5-0-prerelease-targets-52265d","commit":"eaef2270c"}
---

# BPR-2209: Settings › Prompts: Add to names the surface of each group

## Found

The owner, 2026-11-15: "I just looked at the prompt library right click menu
and there's no information on where we're adding it to". The library row's
Add to submenu listed "Ask menu / Suggested chips" three times between
dividers, then a greyed "Tour stop / Tour part / Whole tour". Nothing said
which surface each group was.

## Cause

`prompts_page::fill_add_to` headed each group with `QMenu::addSection`. Qt
6.12 shows a macOS context menu as a native menu, where a section is a bare
divider and its title is dropped.

## Fix (`eaef2270c`)

- Each group is headed by a disabled, bold title item: Pull request, File,
  Thread, Tour, in that order, after a divider. The items keep their words
  under it ("Ask menu", "Suggested chips"; "Tour stop", "Tour part", "Whole
  tour"). Check marks, the disabled state and the reason tooltips are
  unchanged.
- The words come from the helpers the Surfaces section now builds its panels
  from (`surface_panel_name`, `surface_list_name`), so the two cannot drift.
  "Tour" is the one new word, defined beside them.
- A group greyed throughout for one reason says it under its items in an
  italic, disabled line ("It was written for another chat."), because a
  native menu may not show a disabled item's tooltip.

## Verification

- `test_prompts_page` `test_add_to_headings`: the library row's Add to has
  headings Pull request, File, Thread, Tour in order, each bold, disabled and
  not checkable, each followed by its items; every surface is a checkable
  choice; the tour items are greyed with a tooltip and one note under them
  repeats it. Passes offscreen (Fusion, the Windows and Linux path) and with
  the native look on macOS, light and dark.
- Native-look renders (Cocoa, QMacStyle, off screen) of the submenu were
  checked: headings, dividers, checks and the italic note read as intended.
  The NSMenu itself cannot be drawn off screen; the bold font reaching it is
  expected from Qt's menu item font sync, not observed.
