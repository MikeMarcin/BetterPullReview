---
schema: "implement-rfc.card.v1"
id: "BPR-1250"
title: "Closing the last window takes Help off the macOS menu bar"
status: "Ready"
rfcs: []
depends_on: []
platforms: ["mac"]
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1250--BPR-1250--34ad467c0959.json","related_cards":["BPR-1035","BPR-1121","BPR-1157"]}
---

# BPR-1250: Closing the last window takes Help off the macOS menu bar

## Observation

At `main` 580589740 on macOS arm64, in an unlocked Aqua session where AppKit
made the window key, `application_profile_empty_desktop_native_menu_bar`
fails at `closing the last window takes no menu off the bar`
(`tests/end_to_end/profile_empty_desktop_native_menu_bar_tests.cpp:415`). The
windowed top-level menus are Apple, File, Profiles, Pull Request, View and
Help; windowless they are Apple, File, Profiles, Pull Request and View. The
trace shows the resident desktop's Help as `~Help[]`, empty and hidden. (An
earlier run in the same pass, with the screen locked, failed the same way.)

The windowed Help is the workbench window's diagnostic menu
(`workbench_window.cpp`, RFC-BPR-0027's exports, 75700f4dd and e0a870b34 on
2026-09-26). The resident menu's Help is `release_update_menu`, whose only item,
Check for Updates…, is hidden on native macOS, so the Mac drops the empty
menu. The trace also shows File losing Open Project… windowless
([[BPR-1157]]); the course compares titles only.

## Outcome

The windowless bar keeps every top-level menu the windowed bar has, with
windowless Help offering what applies without a window. Which diagnostic
commands belong there is a UX choice to settle before implementing.

## Direct acceptance

- `application_profile_empty_desktop_native_menu_bar` passes in the owner's
  Aqua session.
