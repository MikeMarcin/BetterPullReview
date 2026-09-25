---
schema: "implement-rfc.card.v1"
id: "BPR-1132"
title: "The document tab strip in the comparison's header"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129", "BPR-1131"]
priority: "P2"
milestone: "Review workbench scaffolding"
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v6.html","criteria":["AC-05","AC-06","AC-07","AC-10"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json"}
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
AC-05, AC-06, AC-07, AC-10 as drawn in [round v6](../../design/BPR-1106-scaffolding-v6.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
