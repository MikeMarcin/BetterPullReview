---
schema: "implement-rfc.card.v1"
id: "BPR-1493"
title: "The desktop bar's View menu differs from a window's"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The desktop bar's View menu differs from a window's

Two rows fail on main at cd4860e30, in the full gate and again alone:

- `qt_profile_management_window`: "with the window menu's own commands, in
  the window menu's own order" ([[BPR-1304]], [[BPR-0796]]).
- `application_profile_management_public_path`: "and carries the window
  menu's View, offered and disabled, so the close takes no menu off the bar".

With no window open, the macOS desktop bar's View menu must offer the same
commands, in the same order, as a window's View menu.

## Acceptance

- AC1: The cause is named with the commit that introduced it. The desktop
  bar's View menu matches a window's again (or the test's expectation is
  corrected, if the window menu legitimately changed), and the test's pinned
  count is consistent.
- AC2: Both rows pass on macOS.

## Boundaries

No menu redesign.

## Evidence

Cause. Three commits on 2026-10-12 and 2026-10-13 added commands to the head
of the window's View menu and mirrored each in `desktop_menu_bar.cpp`. Two of
those copies did not match the window:

- `40639f2f6` ("fix(review): enter exact Review context through file and
  change navigation", RFC-BPR-0076) introduced the failure. It gave the desktop
  bar `Previous file`, `Next file`, `Previous change` and `Next change` without
  the window's mnemonics (`&Previous file`, `&Next file`, `Pre&vious change`,
  `Ne&xt change`), so the texts differ. That is the assertion
  `qt_profile_management_window` fails on.
- `a7fb9af0a` ("feat(find): compose native pane source search", RFC-BPR-0077)
  left the desktop's Find submenu entry enabled, so the public path's
  "offered and disabled" check also fails. It also listed the submenu's
  commands in a different order from the window's (Find, Find in Files, Next
  pane match… instead of Find, Next pane match, Previous pane match, Find in
  Files, Next PR result, Previous PR result).
- `720c52c69` (Go to header/source, RFC-BPR-0074), `40639f2f6` and `a7fb9af0a`
  each changed the View menu without updating the Qt test's pinned layout. The
  test still pinned 15 items with `&Line numbers` at index 0, while both menus
  now have 23 items.

Fix (product). `src/adapters/qt/src/desktop_menu_bar.cpp` now uses the window's
navigation texts, mnemonics included. It lists the Find submenu's commands in
the window's order, with the macOS Ctrl+G and Ctrl+Shift+G secondary keys still
on Next and Previous pane match, and disables the submenu entry itself.
`tests/adapters_qt/profile_management_window_tests.cpp` pins the 23-item layout:
it adds checks for the leading Find, navigation and Go to header/source items
and checks that Find holds the window's Find commands in order, each disabled.
Every other check is kept and only moved to the new index. No assertion was
relaxed. The trigger-for-no-effect list now also covers the new items. No menu
was redesigned and `src/app` is unchanged.

Before (branch at `a8fd17aea`, the main tree the card was claimed on):

```text
ctest --preset dev -R '^(qt_profile_management_window|application_profile_management_public_path)$' --output-on-failure
418 qt_profile_management_window ............ Failed
    FAILED: with the window menu's own commands, in the window menu's own order
644 application_profile_management_public_path ... Failed
    FAILED: and carries the window menu's View, offered and disabled, so the close takes no menu off the bar
0% tests passed, 2 tests failed out of 2
```

After:

```text
ctest --preset dev -R '^(qt_profile_management_window|application_profile_management_public_path|qt_menu_bar_status_item|qt_menu_bar_status_item_cocoa|application_profile_empty_desktop_native_menu_bar|qt_hide_drawers_native_menu_key)$' --output-on-failure
298 qt_menu_bar_status_item ............................ Passed
299 qt_menu_bar_status_item_cocoa ...................... Passed
418 qt_profile_management_window ....................... Passed
644 application_profile_management_public_path ......... Passed
645 application_profile_empty_desktop_native_menu_bar .. Passed
646 qt_hide_drawers_native_menu_key .................... Passed
```
