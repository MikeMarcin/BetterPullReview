---
schema: "implement-rfc.card.v1"
id: "BPR-1581"
title: "Show a thread selected in Review in Discuss, and back"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner request 2026-10-17","owner_note":"Why can't I go from a selected comment in Review to see it in Discuss easily?","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1212","BPR-1290","BPR-1293","BPR-1356","BPR-1383","BPR-1384","BPR-1542","BPR-1567","BPR-1582"],"reservation":"docs/issues/_reservations/BPR-1581--BPR-1581--c1269e5136e4.json"}
---

# BPR-1581: Show a thread selected in Review in Discuss, and back

The owner asked on 2026-10-17: "Why can't I go from a selected comment in
Review to see it in Discuss easily?"

## Why it is missing

Every road between the thread surfaces was built except this one:

- **Threads → Review** is the pick [[BPR-1293]] built (click, Return), and
  **Threads → Discuss** the row's Discuss button, Shift+Return and the row
  menu [[BPR-1356]] added. Both go through `core::route_workbench_thread`.
- **Discuss → Review** is a card's *Open in Review* button, thread menu item
  and comment context-menu item ([[BPR-1383]], [[BPR-1384]]). It is disabled
  for an outdated thread and for a pull request comment.
- **Review → Discuss** exists only for a draft being written: the drawer's
  Maximize, ⌘⇧M ([[BPR-1212]]). A published thread's card in Review offers
  Show line, Resolve, Mark read, More and Close, and its menu (More, right
  click, Shift+F10) offers Reply, Mark read, Resolve, Show line and Copy first
  post. The code's menu on a line with a thread offers Open thread. None of
  them goes to Discuss. ⌘3 switches to Discuss without the thread.

No card or RFC designed, deferred or refused a Review → Discuss jump. The
direction of the October work was the other way: [[BPR-1290]] made the
drawer the place a file's thread is read beside its code, and [[BPR-1293]]
moved the `#discussion_r` link, the published-post reveal and Previous and
Next defect *out of* Discuss into Review in place, keeping Discuss for what is
not on a file (the owner's Q2, 2026-10-06: "just keep discuss for now"). Each
later card added the road its owner note asked for, and nobody asked for this
one until now.

## Design

Designed with the `frontend-design` skill on the cards' own language: amber
is where you read (the pick's wash and 3-pixel edge), blue is where you
write. One shared route decides every move between the surfaces, the same one
[[BPR-1582]]'s Threads gestures use.

- **Show in Discuss** on a thread in Review: the thread menu (More, right
  click, the menu key, Shift+F10) of the open card and of a closed note; the
  context menu of a post in the open card (that post is the target); and the
  code's context menu on a line with a thread, after Open thread.
- **⌥⌘3** (`Ctrl+Alt+3`), *Show the thread in Discuss*, next to ⌘3 =
  Discuss: the open thread at its picked post, else the drawer's current
  thread, else the selected line's thread. **⌥⌘2** (`Ctrl+Alt+2`), *Show the
  thread in Review*, is the same move back from Discuss, next to ⌘2 = Review.
  Both are View menu items, mirrored disabled on the resident menu bar, listed
  in Help → Keyboard Shortcuts, and step aside while typing (Ctrl+Alt is AltGr
  on Windows, where AltGr+2 and AltGr+3 type `@`, `²`, `#` or `³`).
- Discuss opens with the thread open at that post, the post selected, in view
  and focused. On arrival the post's wash is drawn stronger with an amber
  ring, held briefly, then dissolves to the pick's resting wash, the same
  arrival in either direction. A dissolve, not motion, so it stands under
  Reduce Motion. Discuss's filters that would hide the thread are turned off
  and said, as [[BPR-1356]] does.
- **Outdated** threads: in Discuss they are listed, so Show in Discuss works.
  Discuss's Open in Review now opens an outdated thread among its file's
  threads that have no line, as a Threads pick does, and says so, instead of
  being disabled.
- **Pull request comments and reviews** are not on a file: Review has none,
  so they have no Show in Discuss, and Open in Review stays disabled with its
  reason.
- **My draft or pending comment**: Discuss lists no draft; Show in Discuss
  opens it in Discuss's composer, as Maximize does, and the status bar says
  so.
- The core decides: which thread and post is the target, where it opens, what
  is cleared, why, and when the move is unavailable. Qt presents.

## Acceptance

- `BPR-1581:core`: `core_workbench_interaction` covers the target choice and
  the route in both directions: Discuss clearing exactly the hiding filters,
  outdated to Review among the unplaced threads, a pull request comment
  unavailable, my draft to Discuss's composer, nothing selected unavailable.
- `BPR-1581:window`: `qt_workbench_window_scale_*` drive the real window:
  Show in Discuss from the open card's menu, a closed note's menu, a post's
  context menu and the code's line menu, and ⌥⌘3, land in Discuss on that
  thread and post, washed, selected, in view and focused; ⌥⌘2 and Open in
  Review come back; an outdated thread both ways; a pull request comment
  offers no Show in Review; a draft opens in the composer. Fails before.
- `BPR-1581:catalogue`: `qt_keyboard_shortcuts_scale_*` list both keys and
  find their live bindings; nothing else binds them.
- `BPR-1581:negative`: a negative control shows the window course refusing a
  move that lands on another thread or loses focus.
- `BPR-1581:gates`: clang-format 22 and clang-tidy on touched files, the
  documentation gate, focused rows and one full `dev` ctest on the merged
  tree.
- `BPR-1581:owner-look` (human): the owner accepts the frozen candidate,
  a frame sequence of the jump in Light and Dark, in a new short session.
