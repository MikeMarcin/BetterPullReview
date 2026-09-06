---
schema: "implement-rfc.card.v1"
id: "BPR-0646"
title: "Workbench chrome: strip, epoch line, modalities and rails"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Workbench chrome: strip, epoch line, modalities and rails

First slice of the owner-locked [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
direction (mockup states 1 to 3). Replaces the 220-pixel review header scroll
area in `workbench_window.cpp` and the toolbar it carries.

## Scope

- The 30-pixel strip as the full-surface asset draws it (full ink, bordered
  buttons, two drawer toggles at the left, modality control alone in the
  centre) with the listed controls; the existing next/previous
  change and file actions move into it with their positions; jump to next new
  comment reads the ledger's unread projection.
- The epoch line over the existing epoch selectors: nodes per upload, labels
  beneath, bracket handles, blue span, amber "my last review" tick with a
  tooltip; drag, click, ⌘-click, right-click menu, hover detail, and keyboard
  stepping of each handle; the named comparisons in a dropdown.
- Modality presets Overview, Read, Review, Discuss with ⌘1 to ⌘4; drawers that
  collapse to 18-pixel rails with a vertical label and an unread count; no
  automatic collapse; a per-window persisted modality.
- The signed-out state shows the device sign-in (code, open browser, cancel)
  in the header without scrolling; the appearance settings panel moves into the
  Settings window drafted in `settings_window.{hpp,cpp}` under `Cmd+,` until the
  RFC-BPR-0018 design replaces it.
- Core decides, host performs, Qt presents; no new authority in Qt. Owned
  paths: `src/adapters/qt` workbench window, session and device panels, epoch
  line and settings window, `src/app` composition wiring, their focused tests
  and the render evidence they pin.

## Acceptance

- RFC-BPR-0028 AC-01, AC-02, AC-03 and AC-07 at 1280x852 and 1920x1080.
- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the public-path courses that pin
  header render evidence pass; the boundary gate passes.
- Attended capture on the Mac candidate showing the strip, epoch line, a
  collapsed and an open rail, and the device sign-in reachable while signed
  out.
