---
schema: "implement-rfc.card.v1"
id: "BPR-0724"
title: "Read mode leaves too little horizontal room for the code"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
---

# Read mode leaves too little horizontal room for the code

The owner, in the v003 verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"Even in read mode viewing experience is not great." Asked what
specifically was wrong, the owner answered on 2026-09-11, verbatim:

> it was difficult to read the code because of a lack of horizontal area
> for the text display

## What the code says

Read is a drawer preset (`workbench_window::choose_modality`,
`src/adapters/qt/src/workbench_window.cpp`): it collapses the files drawer,
expands the conversations drawer and leaves the repositories drawer in
whatever state it has. The comparison is two whole-document panes side by
side (`diff_workbench.cpp`, `Comparison->setSizes({ 500, 500 })`, no line
wrap). The conversations drawer has a 375-pixel minimum
(`conversation_drawer_default_width`) and the repositories drawer opens at
300 by default.

At the owner's window, about 1,390 points wide, Read with the repositories
drawer open leaves the comparison about 715 points, so each pane shows
roughly 45 columns of code at the default code font; with the repositories
drawer collapsed by hand (the owner's screenshots show it collapsed) each
pane gets roughly 490 points, about 60 columns. That is the lack of
horizontal area.

## Scope

Two parts. The first is bounded and lands here; the second is the owner's
choice, asked in the next attended session.

1. Read collapses the repositories drawer as Overview does. The repositories
   tree is not part of reading a file and was never part of the Read
   preset; RFC-BPR-0028 says Read "collapses the files drawer and keeps
   comments beside the code" and says nothing about repositories. The
   strip's toggle reopens it. The preset test pins the three exact intents.
2. What Read does with the remaining width is a requirement decision the
   RFC settles the other way today ("keeps comments beside the code",
   AC-05 "Read shows this file's threads aligned to their lines"), so it
   is not changed here. The attended candidate offers the owner:
   - Read also collapses the conversations drawer, so the comparison takes
     the whole width and the strip's toggle brings the threads back
     (amends the RFC's Read definition and AC-05's wording).
   - Keep the conversations drawer in Read but let it go narrower than
     375 pixels.
   - A single-pane layout for Read (one document with the other side's
     changes marked), which is a new diff surface and would be a new RFC
     slice.
   - Leave Read as it is after part 1.

Owned paths: `src/adapters/qt/src/workbench_window.cpp`,
`tests/adapters_qt/workbench_window_tests.cpp`, RFC-BPR-0028's modality
paragraph.

## Acceptance

- Choosing Read dispatches exact set-drawer intents: files collapsed,
  conversations expanded, repositories collapsed; the other presets are
  unchanged. `qt_workbench_window_scale_100` pins it.
- RFC-BPR-0028's modality paragraph says Read collapses the repositories
  drawer.
- The owner chooses, on the signed bundle in the next attended session,
  which of the four options above Read should take next, or accepts Read
  as it then is.
