---
schema: "implement-rfc.card.v1"
id: "BPR-1132"
title: "The document tab strip in the comparison's header"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129", "BPR-1131"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-05","AC-06","AC-07","AC-10"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1132: The document tab strip in the comparison's header

## Outcome

The comparison's header becomes the tab strip.

- Names elide from the left, where file names differ.
- A tab with its own pair shows it on its label.
- At a platform's width floor the strip shows the active tab and a count of
  the rest.
- The tab's menu offers Duplicate, Close, Close others, returning to the
  window's comparison, and making the tab's pair the one new tabs take.
- Cmd-click (Ctrl-click) in the Files tree and Open in New Tab open a tab.
- Ctrl+Tab and Ctrl+Shift+Tab move between tabs, and Cmd-W (Ctrl+W) closes
  one. If Cmd-W closes the window today, the window moves to Cmd-Shift-W
  (Ctrl+Shift+W).

The Updates line shows the active tab's pair, and the review bar's controls
act on the active tab. Overview and Discuss show the window's comparison. A
tab over an older pair places only its own revisions' threads, says how many
are on later updates, and offers to show them.

## Acceptance

- The composed course opens a file in a new tab, compares two pairs of one
  file side by side, closes, restarts and returns.
- The tab keys are unique in the assembled window.
- Tabs meet the focus, accessibility-name and 100 to 200 percent scale
  checks, at narrow and wide windows.
- The older-pair tray notice appears and routes to the later threads.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-05, AC-06, AC-07, AC-10 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Checkpoint: in progress, stopped at the owner's wind-down (2026-09-25)

Claimed at `9d1b17a4`. No product code for this card is on main. The work in
progress is a patch against `c0fdd70a`, with what it holds, its state and how
to resume it:
[wip/README.md](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/wip/README.md)
(`BPR-1132-document-tab-strip.patch`, SHA-256
`830375e7239d9328585f0ba6dbbb3758200fa26288f6cb6c76fc37ba3f53eb0c`).

- **Built on the patch:** the tab strip in the comparison's header (in place of
  the diff status row), the tab menu and "+", the width floor's "+N", Cmd-click
  and Open in New Tab in the Files tree, the tab keys (Ctrl+Tab, Ctrl+Shift+Tab,
  Cmd-W; nothing had Cmd-W before, so no window command moves), the older-pair
  notice with Show them, Comment disabled off the current head, and the
  composition's per-window tab host (presenting loads through the epoch and
  workbench hosts, Overview and Discuss on the window's comparison,
  persistence, Resume, reset, shutdown).
- **Measured on the patch (macOS, 1280x1000, offscreen):** 872 folded and 838
  open, with the 28-pixel strip where the 15-pixel status and 6 of spacing were.
- **Passing on the patch:** the build, the window and focus-order courses at
  100 percent, the UIforETW capture at 872 and 838, and nine composed and
  end-to-end courses (listed in the README).
- **Not yet:** the Qt tests for the strip, keys, gestures, notice and Comment;
  the composed course (open in a new tab, two pairs of one file, close,
  restart, return); the reseal of the two `github_application.cpp` pins; format,
  tidy, the specification and this card's record.
- **Owner asks prepared for when it is done** (one question per criterion):
  1. The "+" at the strip's end opens the active tab's exact view in a new tab.
     Is that what the design's "+" should do?
  2. On a tab over an older pair the Comments tab shows "No threads on Feb 4 →
     Feb 9. Two threads are on later updates of this file." with Show them, above
     the thread list, where the later threads stay listed but unplaced. Should
     they be hidden until Show them?
- **Next step:** apply the patch at the current main, finish the list above,
  then the m2 integration pass.
