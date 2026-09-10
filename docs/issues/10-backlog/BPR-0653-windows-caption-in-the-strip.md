---
schema: "implement-rfc.card.v1"
id: "BPR-0653"
title: "Windows: the strip as the title row"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Windows: the strip as the title row

[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) makes the
strip the row the window controls sit in. BPR-0646 delivers the macOS form
through Qt's expanded client area (the traffic lights in the strip's left
inset, the title text hidden, empty strip space dragging and zooming the
window). The Windows form follows Visual Studio Code, the owner's closest
reference on that platform, and is platform work of its own so the portable
strip never carries it.

## Scope

- The workbench window on Windows draws no system title bar. The strip
  carries, at its left, a compact menu control standing in for the menu bar
  rows (the File, View and Review menus the window already builds), then the
  same regions as on macOS; at its right, after settings, the minimise,
  maximise-or-restore and close buttons in the platform's current caption
  style, 46 pixels wide each, in the strip's ink.
- Snap layouts on hover of the maximise button, dragging by empty strip
  space, double-click to maximise and restore, Aero shake and edge snapping
  keep working: the window answers `WM_NCHITTEST` with `HTCAPTION` for empty
  strip space and the caption-button codes for the three buttons, through a
  native event filter in `src/app` (host performs) rather than by moving the
  window by hand.
- The frame extends into the client area so the platform's rounded corners,
  shadow and resize borders remain; the strip's top edge keeps the one-pixel
  resize band usable.
- The strip's reading order, focus order and accessibility names stay the
  ones BPR-0646 pinned; the caption buttons are the last three stops.
- Owned paths: the Windows branch of `src/app` composition, a Windows-only
  presenter for the caption buttons in `src/adapters/qt`, the Windows
  registration inventory and its focused tests.

## Acceptance

- RFC-BPR-0028 AC-01 on Windows at 1280x852 and 1920x1080: the chrome above
  the comparison is the strip and the epoch line, and the strip is the row
  the caption buttons sit in.
- Snap layouts appear on hovering the maximise button; dragging empty strip
  space moves the window; double-click maximises and restores.
- The Windows station's focused Qt selectors and the manifest gate pass; the
  new uncontained registration, if any, is pinned by name in the same commit.
- Attended capture on the Windows station showing the strip with the caption
  buttons and the snap layout flyout.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.
