---
schema: "implement-rfc.card.v1"
id: "BPR-0662"
title: "Drawer widths survive Home and a squeezed splitter"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Drawer widths survive Home and a squeezed splitter

Opening a pull request in the signed `app-release` bundle at the pack head
(bc551240, 2026-09-07, 1280x820) showed the review workbench with all three
drawers 64 pixels wide and the centre taking the rest. The owner's
`workbench-settings.sqlite3` held `files_width`, `conversations_width` and
`repositories_width` at 64, the host's floor, written on 2026-09-07 00:11
local by the Home build.

Cause. `workbench_drawer_panel::resizeEvent` reported every width change as
the reviewer's, and the window only suppressed reports while it was itself
applying a published layout. Since BPR-0646 the workbench splitter is a
hidden page of the centre stack while Home shows, so it is laid out at its
default size, its drawers squeezed to their 64-pixel minimum, and those
widths were reported and persisted before the page was ever shown. A
window narrower than the persisted widths did the same. Neither is a width
a reviewer chose, and RFC-BPR-0028 AC-03 says the drawers never change on
their own.

## Scope

- A width reaches the host only from a drag of a splitter handle
  (`QSplitter::splitterMoved`); the drawer panel reports nothing from its
  own resize.
- Whenever the splitter itself is resized or shown, the window re-applies
  the persisted widths and the centre absorbs the difference, so a squeeze
  is temporary and never written back.
- A persisted width beneath the presented floor (160 pixels) was written by
  a squeeze; the drawer opens at its design width (files 320, conversations
  375, repositories 300) and nothing is reported until the reviewer drags.
- The UIforETW probe and the window tests drive the handle the way a
  reviewer does, through the splitter's own move.
- Owned paths: `src/adapters/qt` workbench window and drawer panel, the
  UIforETW probe in `src/app/src/github_application.cpp`, their tests and
  the application digests they pin.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the UIforETW course pass; a
  published layout applied while the window is hidden, a window narrower
  than the persisted widths, and a return to a wider window report no width
  and end at the persisted widths; a handle drag reports exactly the dragged
  drawer's width; a layout persisted at the floor opens at the design
  widths.
- Observed in the signed bundle: the workbench opens with the drawers at
  usable widths after the clobbered layout.
