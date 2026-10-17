---
schema: "implement-rfc.card.v1"
id: "BPR-1586"
title: "Show a thread selected in Review in Threads, beside Show in Discuss"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner note on accepting BPR-1581 v001, 2026-10-17","owner_note":"We should also be able to go from rview to show in threads because why not","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-1293","BPR-1356","BPR-1567","BPR-1581","BPR-1582"],"reservation":"docs/issues/_reservations/BPR-1586--BPR-1586--61e6e8a52fa6.json","builds_on":"BPR-1581 branch bpr-1581-thread-navigation; lands with it in one integration"}
---

# BPR-1586: Show a thread selected in Review in Threads

The owner accepted [[BPR-1581]] v001 on 2026-10-17 with the note "We should
also be able to go from rview to show in threads because why not". The
coordinator scoped it as a small follow-up that matches the accepted pattern
and lands without a new review once the tests prove it.

## Scope

- **Show in Threads** next to Show in Discuss wherever Show in Discuss appears
  in Review: the thread menu of an open card or closed note, a comment's own
  menu, the code's menu on a line with a thread, and my draft's note.
- **⌥⌘4** (`Ctrl+Alt+4`, next to ⌘4 = Threads), *Show the selected thread in
  Threads*: a View menu item, mirrored on the resident menu bar, listed in Help
  → Keyboard Shortcuts, stepping aside while typing, like ⌥⌘3 and ⌥⌘2.
- Threads opens with that thread's row (at the named post) as the cursor's,
  selected and scrolled into view, with the keyboard on the list. The core's
  shared move decision (`route_workbench_thread`, destination Threads) decides
  it; a comment that is not on a file has no row and nothing moves. My draft
  has its own row.

## Acceptance

- `BPR-1586:core`: `core_workbench_interaction` covers the route to Threads
  from Review: on a file, a draft and a comment not on a file.
- `BPR-1586:window`: `qt_thread_moves_scale_*` drive each entry point and
  ⌥⌘4 into Threads on that row and post, focused; a negative control (an
  unknown thread, and the key while typing) moves nothing.
- `BPR-1586:catalogue`: `qt_keyboard_shortcuts_scale_*` find the key bound
  once.
- `BPR-1586:gates`: format, tidy, documentation gate, and the full `dev`
  ctest shared with the [[BPR-1581]] landing.
