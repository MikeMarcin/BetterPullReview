---
schema: "implement-rfc.card.v1"
id: "BPR-1136"
title: "The Windows strip's Application menu shows no items to accessibility when it opens again"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-0974's cleared AC2 course on 85e522d8, 2026-09-25 (bpr-main-win, segment 11)","reservation":"docs/issues/_reservations/BPR-1136--BPR-1137--8ee928da6b11.json","related_cards":["BPR-0974","BPR-1028","BPR-0653"]}
---

# BPR-1136: The Windows strip's Application menu shows no items to accessibility when it opens again

## Finding

[[BPR-0974]]'s AC2 course ran on the cleared Windows image `85e522d8`. It
stopped at the second use of the strip's Application menu. The menu was open
on screen, but UI Automation found no item in it, by posted click and by
`InvokePattern` alike. The first opening, on a fresh window, exposes its items
every time. A screen reader would find the reopened menu empty. That card
records the stop and the window list.

## Cause

`workbench_window::open_strip_menu` (`src/adapters/qt/src/workbench_window.cpp`)
builds a new `QMenu` each time, from the menu bar's own actions. It deletes
the menu on close ([[BPR-1028]]). Qt's accessibility cache keys a menu
item's interface by its `QAction`. The menu bar's actions outlive each strip
menu, so a menu opened after one that had been read finds its predecessor's
item interfaces, still cached. Those interfaces are invalid and parented to
the deleted menu.

An in-process probe (offscreen, not committed) read the menu's accessible
children on two openings after a first:

- on the first read, five items, each valid, named, and the open menu's own;
- on the next, the same five unique ids, each `valid=0` and not the open
  menu's child.

That is the empty menu UI Automation reported.

## Scope

- Before a strip menu adds an action, drop that action's cached accessible
  interface, so the new menu makes its own.
  - This adds no new menu lifetime: the menu is still a heap child that deletes
    itself on close, as [[BPR-1028]] requires.
- Pin it in the Windows `qt_workbench_strip_menu` row, with a negative
  control.
- Real-platform proof is not this card's. A new cleared tests-disabled image
  and BPR-0974's AC2 course under UI Automation are BPR-0974's.

## Acceptance

- AC1: The cause is shown in-process: a reopened strip menu's accessible
  items are the deleted menu's, invalid and unparented.
- AC2: With the fix, `qt_workbench_strip_menu` passes on Windows. It now
  opens the menu twice more after a first, and requires every accessible item
  to be valid, named and the open menu's own. With the product change reverted
  and the new check kept, the row fails at that check.
- AC3: `architecture_boundaries` and the documentation gate pass.
