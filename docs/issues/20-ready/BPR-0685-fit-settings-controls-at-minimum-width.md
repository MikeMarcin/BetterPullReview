---
schema: "implement-rfc.card.v1"
id: "BPR-0685"
title: "Fit Settings controls at minimum width"
status: "Ready"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0684"]
---

# Fit Settings controls at minimum width

## Scope

BPR-0683's actual 760x480 workbench Settings captures reveal 46px horizontal
scroll on Accounts and 21px on Reviews in all four appearances. Repair the
narrow Settings sidebar and control layout using measured label widths and
wrapping/stacking. Keep existing controls, actions and dispatch identities.

Owned paths: Qt Settings/session/workbench layouts and Settings render tests.
Routing: standard | Reason: default implementation.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-07/08/09: all seven pages fit 760x480 without horizontal scrolling at
100/125/150/200%, all four appearances; page/row identity and keyboard access
survive resizing. Long metadata remains accessible in bounded controls.

## Evidence

Initial deterministic macOS arm64 Qt 6.10.3 captures in build/dev/settings-review
were inspected and show the horizontal overflow. Those are discovery evidence,
not passing acceptance. BPR-0683 is paused in Backlog while this repair runs;
its render-test changes are now owned by this slice.
