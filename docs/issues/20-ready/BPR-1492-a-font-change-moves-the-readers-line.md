---
schema: "implement-rfc.card.v1"
id: "BPR-1492"
title: "A font change or zoom moves the reader's line"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# A font change or zoom moves the reader's line

Two rows fail on main at cd4860e30, in the full gate and again alone:

- `qt_appearance_profile`: "the reprojected position keeps its comparison
  and visible reading line within one row" (BPR-1336 C4).
- `application_diff_zoom_public_path`: "the reader's line stays where it
  was: 249 to 271".

Both say a font change (appearance or diff zoom) no longer keeps the visible
reading line in place.

## Acceptance

- AC1: The cause is named with the commit that introduced it, and the fix
  keeps the reader's visible line within one row across a font change and a
  zoom, as BPR-1336 C4 requires, without loosening either assertion.
- AC2: Both rows pass on macOS, and the neighbouring scroll and zoom rows
  still pass.

## Boundaries

If the product behaviour is correct and a test reads it wrongly, say so
plainly and fix the test only.
