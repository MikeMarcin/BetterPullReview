---
schema: "implement-rfc.card.v1"
id: "BPR-1291"
title: "The shared diff scroll bar moves while the panes stand still, and the panes cannot scroll past their end"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1291--BPR-1291--15e229b6f060.json","related_cards":["BPR-0208","BPR-1264","BPR-1286"],"owner_references":["docs/design/owner-refs/BPR-1291-owner-ref-bpr-read-scroll.webp","docs/design/owner-refs/BPR-1291-owner-ref-vscode-before-scroll.webp","docs/design/owner-refs/BPR-1291-owner-ref-vscode-after-scroll.webp"],"acceptance":["BPR-1291:cause","BPR-1291:bar-agrees","BPR-1291:past-end","BPR-1291:aligned","BPR-1291:kept","BPR-1291:large-review"]}
---

# BPR-1291: The shared diff scroll bar moves while the panes stand still, and the panes cannot scroll past their end

## Origin

The owner, in chat on 2026-10-06, verbatim:

> It's weird that the main diff pane has a scroll bar that moves when the
> content doesnt scroll I would expect it to work like vscode where you can
> scroll until only the last line is visible

Their screenshots (fixture and repository content only):

- [`BPR-1291-owner-ref-bpr-read-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-bpr-read-scroll.webp):
  Read on `renderer.cpp` from the `nested-folders-v1` fixture, 8 lines on the
  left and 15 on the right. The shared scroll bar right of the right pane has
  a thumb that moves, but neither pane scrolls.
- [`BPR-1291-owner-ref-vscode-before-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-vscode-before-scroll.webp)
  and [`BPR-1291-owner-ref-vscode-after-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-vscode-after-scroll.webp):
  VS Code on a 5-line file before and after scrolling. With scroll-past-end
  it scrolls until line 5, the last, is the only line left, at the top.

## Outcome

- The shared bar has range only where some pane moves. Its minimum shows
  both files from their first line, its maximum shows each file's last line
  at the top, and every step between moves at least one pane.
- Both panes scroll past their end the way VS Code does: a short file, even
  one shorter than the viewport, scrolls until its last line is at the top.
- The two sides stay aligned through the connector: the ribbons keep joining
  the same lines on both sides at every position, past the end included.
- Unchanged: BPR-1264's staged publication, BPR-1286's `settle_geometry`,
  change navigation, the placement of a revealed change on the
  synchronization rail, and the 100 ms input targets.

## Acceptance

- **BPR-1291:cause.** The card records why the bar had range while the panes
  did not scroll.
- **BPR-1291:bar-agrees.** A Qt test on the production workbench shows that
  the bar's minimum and maximum are the panes' first and last positions and
  that no bar step leaves both panes still. A negative control shows that the
  check sees a pane moved behind the bar's back.
- **BPR-1291:past-end.** A Qt test shows a file shorter than the viewport
  scrolling until its last line is at the top, through the bar, the wheel
  and End.
- **BPR-1291:aligned.** A Qt test shows the connector's ribbons joining the
  panes' own line positions on both sides at every sampled position,
  including past the end.
- **BPR-1291:kept.** The diff workbench, scroll controller, link map,
  workbench window and syntax composition rows pass on macOS `build/dev`.
- **BPR-1291:large-review.** The 18 large-review rows pass with budgets
  enforced in an optimized tests-on macOS tree.
