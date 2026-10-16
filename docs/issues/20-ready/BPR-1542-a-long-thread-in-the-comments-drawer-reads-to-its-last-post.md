---
schema: "implement-rfc.card.v1"
id: "BPR-1542"
title: "A long thread in the Comments drawer reads to its last post, as in Discuss"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-16","owner_note":"I had an odd experience with a particularly long comment thread where the review tab drawer view wouldn't let me scroll any further down and I couldn't read the rest of the thread except by going to Discuss.","owner_detail":"It was the Comments drawer, reached from Threads: a thread opened from Threads into Review with the drawer showing it.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1293","BPR-0648","BPR-1476"],"reservation":"docs/issues/_reservations/BPR-1542--BPR-1542--5f66449df3ce.json"}
---

# BPR-1542: A long thread in the Comments drawer reads to its last post, as in Discuss

The owner reported on 2026-10-16: "I had an odd experience with a particularly
long comment thread where the review tab drawer view wouldn't let me scroll any
further down and I couldn't read the rest of the thread except by going to
Discuss." It was the Comments drawer (⌥⇧2), reached from Threads: a thread
picked there opens in Review with the drawer showing it.

Governing design: the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md); the
inline Comments drawer is [[BPR-1290]]'s, entered from Threads by
[[BPR-1293]]. The [workbench specification](../../ux/workbench-specification.md)
says the drawer moves with the code, a wheel over it scrolls the code, and
"only when the code can scroll no further does the drawer take the overflow,
so the foot of a tall open thread can still be read."

## Acceptance

- AC1: In the Comments drawer beside the code, an open thread far taller than
  the drawer, entered from Threads, reads to its last post by wheel (and so
  trackpad and the code's scroll bar, which move the same code): every wheel
  notch moves the thread's foot toward the drawer until it shows, while the
  code still has room, and the thread keeps hanging exactly from its line. It
  holds at the drawer's narrowest and widest widths, at two code zooms, in one
  pane, after the open thread grows by six posts, at 100, 125 and 200 percent
  scale.
- AC2: A Threads pick of the thread's last post brings that post into the
  drawer, and Tab reads the thread post by post, each reached part in view, to
  the last post.
- AC3: A negative control with the old geometry fails: Qt's stand-in for a
  line beyond its reach put back, the thread stops while the code scrolls and
  its foot never shows; and the course fails against the unfixed tree.
- AC4: Discuss, the existing inline drawer course and the keyboard reveal of
  [[BPR-1476]] are unchanged; format, tidy and the documentation gate pass.

## Root cause

The open thread hangs from its line: the window's line locator asks the
comparison where the line is (`diff_workbench::line_rect`), and a wheel over
the drawer scrolls the code first, so the thread rises only as fast as its
line. `line_rect` read the pane's `QPlainTextEdit::blockBoundingGeometry`.
Qt places a block by walking from the first visible block and stops about one
viewport above it (two below); for a block beyond that it returns a stand-in
near where it stopped. Once a long thread's line had scrolled about a drawer's
height above the code, its reported position stopped moving, so the thread
stopped while the code went on scrolling under it, and the rest of the thread
only came up once the file ran out, which in a long file is never in practice.
Discuss is not aligned with the code and scrolls its own range, so it showed
the whole thread. Entering from Threads made it likely: the pick puts the
line near the top of the code, so a few notches take it past Qt's reach.

[[BPR-1476]] had met the same Qt limit for the keyboard's row reveal and added
`keyboard_row_rect`, exact however far a row is (the measured axis's line
origins in two panes, the row stride in one), but the drawer's locator never
used it.

Measured before the fix with this card's course: at notch 23 the thread's foot
went from 8387 to 8399 pixels below the drawer's top while the code stood at
6946 of 100000; in one pane at notch 24 it stayed at 8010 with the code at row
224 of 2999.

## Fix

`diff_workbench::line_rect` takes its row from the exact row geometry
(renamed `exact_row_rect`, shared with the keyboard reveal), in two panes and
one. The drawer, its scroll handling and Discuss are unchanged. No core, host,
provider or `src/app` change.
