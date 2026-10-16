---
schema: "implement-rfc.card.v1"
id: "BPR-1542"
title: "A long thread in the Comments drawer reads to its last post, as in Discuss"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-16","owner_note":"I had an odd experience with a particularly long comment thread where the review tab drawer view wouldn't let me scroll any further down and I couldn't read the rest of the thread except by going to Discuss.","owner_detail":"It was the Comments drawer, reached from Threads: a thread opened from Threads into Review with the drawer showing it.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1293","BPR-0648","BPR-1476"],"reservation":"docs/issues/_reservations/BPR-1542--BPR-1542--5f66449df3ce.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"032d0e0c9","evidence":{"kind":"command","detail":"qt_composed_long_thread_scale_{100,125,200}: a 24-post thread entered Review -> Threads -> pick reads to its foot by wheel, every notch moving the foot up, the thread hanging at its line; widths 280 and 760, code zooms 200% and 70%, one pane, after six posts arrive."}},{"criterion":"AC2","outcome":"pass","revision":"032d0e0c9","evidence":{"kind":"command","detail":"Same course: a Threads pick of the last post brings it into the drawer; Tab reaches the last post with each reached part in view."}},{"criterion":"AC3","outcome":"pass","revision":"032d0e0c9","evidence":{"kind":"command","detail":"In-course control with Qt's stand-in stalls; unfixed diff_workbench.cpp fails exact placement and stalls at notch 23 (foot 8387 -> 8399, code 6946 of 100000); one-pane-only unfixed stalls at notch 24."}},{"criterion":"AC4","outcome":"pass","revision":"032d0e0c9","evidence":{"kind":"command","detail":"30/30 focused Qt/application tests incl. qt_composed_github_scale_*, qt_composed_inline_drawer_scale_*, qt_diff_workbench_scale_*, qt_workbench_window_scale_*; 9/9 composition contracts (no reseal); clang-format 22 and tidy clean; DOCUMENTATION_OK."}}]
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

## Evidence

Fix `032d0e0c9` on branch `bpr-1542-long-thread-drawer` (worktree
`/Users/jaeger/.codex/worktrees/bpr1542-long-thread/BetterPullReview`), on
`main` at `da290965b`. Changed: `src/adapters/qt/src/diff_workbench.cpp`,
`src/adapters/qt/include/bpr/qt/diff_workbench.hpp`,
`tests/adapters_qt/composed_github_tests.cpp` (`--long-thread`),
`tests/adapters_qt/CMakeLists.txt` (`qt_composed_long_thread_scale_{100,125,200}`),
`docs/ux/workbench-specification.md`. No `src/app` change.

| Acceptance | Evidence | Result |
|---|---|---|
| AC1 | `qt_composed_long_thread_scale_{100,125,200}`: a 24-post thread (paragraphs, an eight-line fenced block wider than the drawer, a list) on line 150 of a 3,000-line file, more than four drawers tall, entered Review → Threads → pick. Every wheel notch moves its foot up until it shows; on entry the foot shows with the code short of its end and no notes overflow (in one pane, short of its end); the thread hangs at its line − 4 less any overflow a pick's reveal took; repeated at drawer widths 280 and 760, at code zooms 200% and 70% (row height checked to change), in one pane, and after six posts are published into the open thread. A line 2,350 rows below the view is placed at exactly 2,350 row heights. | pass |
| AC2 | Same course: a Threads pick of the last post brings it into the drawer within its reveal; Tab from the drawer reaches the last post with every reached part of the thread in view. | pass |
| AC3 | In-course control: Qt's stand-in put back through the drawer's locator, the foot stops while the code scrolls and never shows. Against the unfixed `diff_workbench.cpp`: fails "a line far below the code's view is placed exactly"; with that assertion skipped, at notch 23 the foot goes 8387 → 8399 with the code at 6946 of 100000 (all three scales); with only the one-pane path unfixed, "in one pane" fails at notch 24 (foot 8010 → 8010, code row 224 of 2999). | pass |
| AC4 | 30/30: `qt_composed_github_scale_{100,125,200}`, `qt_composed_inline_drawer_scale_{100,125,200}`, `qt_composed_long_thread_scale_*`, `qt_composed_resident_drawer_filters`, `qt_conversation_cards_view`, `qt_conversation_finder_scale_*`, `qt_diff_workbench_scale_*` (BPR-1476's keyboard reveal), `qt_workbench_window_scale_*`, `qt_workbench_conversation_reveal`, `qt_workbench_line_draft`, `qt_feedback_label_threads`, `qt_github_conversation_panel`, `application_diff_workbench_public_path`, `application_diff_zoom_public_path`, `application_review_navigation_entry`, `application_thread_words_public_path`, `application_threaded_reply_contract`, `application_active_changed_scaling`; the nine composition contracts 9/9 (no reseal needed); clang-format 22.1.8 clean on the touched C++; `tidy.py --warnings-as-errors` clean on `diff_workbench.cpp` and `composed_github_tests.cpp`; `DOCUMENTATION_OK`. | pass |

No human acceptance is required: every criterion is a measured behaviour of
the composed window, and the drawer's look is unchanged.
