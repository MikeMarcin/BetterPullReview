---
schema: "implement-rfc.card.v1"
id: "BPR-0674"
title: "Overview is starved by the drawers"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Overview is starved by the drawers

The owner, 2026-09-07: "Overview mode should have all drawers collapsed by
default, or maybe always. Those drawers don't really make sense in overview
imo."

Their capture shows why. In Overview at 1280 wide, the repositories drawer
(empty), the files drawer and the conversations drawer take most of the
window and the overview itself is a column about 150 pixels wide. Its own
headings are truncated to "Techni…etails" and "Compari…hole PR", the
since-my-last-review list clips at "My unresolved threa", and the
description and file table are unreadable. The page RFC-BPR-0028 gives the
whole centre to is the narrowest thing on screen.

The modality presets already decide drawer state per modality
([BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md),
AC-03). Overview's preset is what is wrong, not the drawers.

## Scope

- Overview collapses every drawer to its rail. Whether a reviewer can then
  open one by hand in Overview is the owner's call: "or maybe always" reads
  as a preference for always, and the card should ask before removing the
  ability.
- The overview page gets a minimum width that its own headings fit in, so
  no arrangement can squeeze it to a column again.
- Owned paths: `src/adapters/qt` workbench window modality presets and the
  overview page, their tests.

## Acceptance

- Choosing Overview collapses the three drawers to rails and the overview
  fills the centre at 1280 wide; a test pins it.
- No heading on the overview is elided at the window sizes the workbench
  supports.
