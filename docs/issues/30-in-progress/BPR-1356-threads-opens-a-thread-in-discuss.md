---
schema: "implement-rfc.card.v1"
id: "BPR-1356"
title: "Threads opens a thread in Discuss as well as in Review"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1356--BPR-1356--8c7bfcbb3a0c.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1293","BPR-1341","BPR-1342","BPR-1328","BPR-1354","BPR-1355"],"implementation_note":"Designed with the frontend-design skill; owner look required"}
---

# BPR-1356: Threads opens a thread in Discuss as well as in Review

## Owner decision

Owner, chat, 2026-10-09, relayed by the coordinator: from the Threads
modality ([[BPR-1293]], renamed by [[BPR-1341]]), add a way to go directly to
Discuss on a thread, in addition to the existing pick that opens it in
Review. The coordinator proposed the design below and the owner approved it,
verbatim: "Do it".

The owner then added, relayed by the coordinator: each thread row also gets a
right-click context menu with both destinations, "Open in Review" (the
default, shown in bold) and "Open in Discuss", each showing its key (Return,
⇧Return), opened from the keyboard too (the menu key, Shift+F10).

## Scope

- A click or Return keeps opening the thread in Review, unchanged.
- A small **Discuss** button on each thread row, shown on the row under the
  pointer and on the cursor's row, never on a file's header or on my draft.
  It takes the end of the row's words, so the people never move. Its tooltip
  is anchored right below it ([[BPR-1083]]'s `anchored_tooltip`): "Open in
  Discuss (⇧↩)".
- Shift+Return (keypad Enter too) opens the cursor's thread, or the post
  under the cursor, in Discuss.
- A right click on a row, or the menu key or Shift+F10 on the cursor's row in
  the list, opens the row menu: **Open in Review**, the default and drawn
  bold, then **Open in Discuss**, each showing its key. The search keeps its
  own menu. On my draft, Open in Discuss is disabled.
- Discuss opens with the thread's card open at the picked post, washed amber
  as the Review pick is, selected, scrolled into view and focused. Review's
  file does not change.
- If Discuss's filters would hide the card (File scope on another file, Only
  new on a thread with nothing new, Hide resolved on a resolved thread, an
  Overview count's list that leaves it out), exactly those are turned off and
  Discuss's notice says so: "To show this thread, Discuss turned off …". The
  notice goes when the reviewer changes a filter or picks again.
- The routing is the core's: `core::route_workbench_thread` takes the
  destination the gesture asked for, whether the row is my draft and which
  filters would hide the thread, and returns the modality, whether Comments
  opens, the filters to clear and a reason. My draft is not in Discuss's list,
  so Discuss asked for it routes to Review with
  `draft_opens_in_review`, said in the status bar.
- Keys are bound once: the finder reads Return, Shift+Return, the menu key
  and Shift+F10 itself; the menu's shortcuts are bound to the menu
  (`Qt::WidgetShortcut`) and no window command holds those keys.

## Acceptance

- `BPR-1356:owner-decision`: the owner's approval and addition are recorded
  above.
- `BPR-1356:core`: `core_workbench_interaction` covers the route: Review with
  Comments, Discuss clearing exactly the hiding filters, the draft's reason.
- `BPR-1356:finder`: `qt_conversation_finder_scale_*` cover the button's
  placement (hover, cursor row, never a header or my draft, left of the
  people at every rendered width), Shift+Return, the row menu and its default,
  the menu key, and the draft's menu.
- `BPR-1356:window`: `qt_workbench_window_scale_*` drive the window through
  the real core: Shift+Return, the button and the menu's Open in Discuss land
  in Discuss on the card, washed, selected, in view and focused, Review's file
  unchanged; the hiding filters are cleared and said, a filter that does not
  hide it is kept; the menu by right click, the menu key and Shift+F10; Open
  in Review and Return open Review; no window command holds the keys.
- `BPR-1356:fails-before`: the window course's Shift+Return check fails on
  the unchanged sources, and the core course does not compile without the
  route.
- `BPR-1356:docs`: the workbench specification describes it and the
  documentation gate passes.
- `BPR-1356:owner-look`: the owner accepts the frozen build in a new short
  attended session.
