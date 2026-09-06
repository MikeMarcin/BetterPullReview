---
schema: "implement-rfc.card.v1"
id: "BPR-0646"
title: "Fixed workbench header with window-level sign-in and a Settings window"
status: "Ready"
rfcs: ["RFC-BPR-0018", "RFC-BPR-0025"]
depends_on: []
priority: "P1"
---

# Fixed workbench header with window-level sign-in and a Settings window

Owner direction, 2026-09-06 (same frontier exception as BPR-0645). The review
header in `workbench_window.cpp` is a `QScrollArea` capped at 220 px whose
content is session panel, device sign-in panel, queue landing, pull-request
stack, PR body editor and the appearance settings panel in that order. Device
sign-in and appearance are below the fold at every ordinary window size, wheel
events over the header land on appearance spinboxes, and the "Get a device
sign-in code" control is unreachable, which is what made the alpha unusable.

## Scope

Replace the scroll area with a fixed header following the accepted RFC-BPR-0025
compact chrome: one row for the target (owner/repository/number or a pasted
URL, Open, Refresh) and one status/identity line. The signed-out state shows
the device sign-in card (code, open browser, cancel) and the enterprise host
entry in the header itself; the pasted-credential prompt is reached only from
the enterprise path. Migrate the appearance settings panel out of the header
into a modeless singleton Settings window (`Cmd+,`) with a sidebar whose first
page is Appearance, per RFC-BPR-0018 AC-03; other pages may be placeholders
naming what will move there. The PR body editor moves to the conversations
drawer top. Core decides, host performs, Qt presents: no new authority in Qt.
Owned paths: `src/adapters/qt` workbench window, session/device panels, a new
settings window, `src/app` composition wiring, their focused tests and the
render evidence expectations they pin.

## Acceptance

- At 1280x852 with details collapsed, sign-in, target entry and Open are
  visible without scrolling; no wheel event over the header changes a setting.
- Signed out, the device flow can be started, its code copied and the browser
  opened from the header; signed in, the header shows account and host.
- The Settings window opens with `Cmd+,`, hosts the migrated appearance panel,
  and appearance changes still reach the comparison live.
- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the public-path courses that pin
  header render evidence pass; the boundary gate passes.
