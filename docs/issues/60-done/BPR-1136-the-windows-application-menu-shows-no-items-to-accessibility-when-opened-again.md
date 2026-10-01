---
schema: "implement-rfc.card.v1"
id: "BPR-1136"
title: "The Windows strip's Application menu shows no items to accessibility when it opens again"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-0974's cleared AC2 course on 85e522d8, 2026-09-25 (bpr-main-win, segment 11)","reservation":"docs/issues/_reservations/BPR-1136--BPR-1137--8ee928da6b11.json","related_cards":["BPR-0974","BPR-1028","BPR-0653"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f826d758","evidence":{"kind":"observation","detail":"In-process probe (offscreen, not committed): after a strip menu that had been read was deleted, the next opening's five accessible items carried the same unique ids but were invalid and not the open menu's children; the first read's items were valid and parented."}},{"criterion":"AC2","outcome":"pass","revision":"f826d758","evidence":{"kind":"command","detail":"qt_workbench_strip_menu passes on Windows (14.84 s) with the change; with the product change reverted and the new check kept it fails at 'each item of a reopened strip menu is valid, named and the open menu's own'."}},{"criterion":"AC3","outcome":"pass","revision":"f826d758","evidence":{"kind":"command","detail":"architecture_boundaries passes (24.16 s); verify_documentation.py reports DOCUMENTATION_OK."}}]
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

## Fixed and pinned — bpr-main-win, 2026-10-01

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, offscreen,
`build/windows-dev` Debug with warnings as errors, at `f826d758`.

- **The change.** `open_strip_menu` now drops each action's cached accessible
  interface before adding it: `QAccessible::queryAccessibleInterface(action)`,
  then `QAccessible::deleteAccessibleInterface`. The menu's lifetime is
  unchanged: it is still a heap child that deletes itself on close, with no
  nested loop.
- **The pin.** `test_strip_menu_goes_with_its_window` opens the menu twice more
  after its first closing. Each time it requires one accessible item per entry,
  and each item to be valid, named and the open menu's child.

| Check | Result |
|---|---|
| `qt_workbench_strip_menu` with the change | pass, 14.84 s; again at the gate run, 14.85 s |
| The same row, product change reverted, check kept (negative control) | fails at `each item of a reopened strip menu is valid, named and the open menu's own`, 0.94 s |
| `architecture_boundaries` | pass, 24.16 s |
| `DOCUMENTATION_OK` | pass |

Real-platform proof, a new cleared image under UI Automation, stays with
[[BPR-0974]]'s AC2.
