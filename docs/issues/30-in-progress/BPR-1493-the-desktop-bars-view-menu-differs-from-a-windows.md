---
schema: "implement-rfc.card.v1"
id: "BPR-1493"
title: "The desktop bar's View menu differs from a window's"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The desktop bar's View menu differs from a window's

Two rows fail on main at cd4860e30, in the full gate and again alone:

- `qt_profile_management_window`: "with the window menu's own commands, in
  the window menu's own order" ([[BPR-1304]], [[BPR-0796]]).
- `application_profile_management_public_path`: "and carries the window
  menu's View, offered and disabled, so the close takes no menu off the bar".

With no window open, the macOS desktop bar's View menu must offer the same
commands, in the same order, as a window's View menu.

## Acceptance

- AC1: The cause is named with the commit that introduced it. The desktop
  bar's View menu matches a window's again (or the test's expectation is
  corrected, if the window menu legitimately changed), and the test's pinned
  count is consistent.
- AC2: Both rows pass on macOS.

## Boundaries

No menu redesign.
