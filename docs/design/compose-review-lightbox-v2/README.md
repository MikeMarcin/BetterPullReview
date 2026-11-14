# Compose review in the lightbox, round 2

Mockups for [BPR-2175](../../issues/30-in-progress/BPR-2175-compose-review-hides-when-navigating.md),
2026-11-14, after the owner rejected [round 1](../compose-review-lightbox-v1/README.md):
"we changed way too much compared with the existing compose review … I would
like the comment box and my action to be on the right or bottom". Not accepted;
nothing is built from them until the owner chooses.

[index.html](index.html) is standalone HTML; `png/` holds offscreen renders of
each option in a wide and a narrow window, light and dark. `today-sheet-*` and
`today-page-*` are the application's own renders of Finish your review today;
`compare-*` sets today's sheet beside option C at the same width.

Every option keeps today's content: the pending comments as today's rows, the
review summary, one-click Comment, Approve and Request changes (no Submit step),
Discard review, "Pull request and account" and Saved comments….

| Option | What it is |
|---|---|
| A | Today's panel in the lightbox, in two columns: the comments on the left, the summary and the verdict buttons on the right. |
| B | One column in today's order, sized to the lightbox; the verdict buttons stay in view while the comments scroll. |
| C | Today's sheet unchanged; the window dims behind it and a click outside closes it. |
| D | A, with each comment's words in full up to four lines, and a suggestion's change shown as its diff. |
| E | A bottom tray across the window, with A's two columns at full width. |

In A, D and E the hint "Double-click a comment to open it in Review;
Shift+double-click opens it in Discuss." moves under the comments box.

Round 1's accepted point holds for every option: while the lightbox is open the
window's title bar stays live (drag, resize, minimise, zoom, full screen), and
only a click elsewhere in the window, Esc, its close button or a mode tab
closes it, keeping its words.
