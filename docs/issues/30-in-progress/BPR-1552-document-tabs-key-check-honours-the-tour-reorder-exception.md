---
schema: "implement-rfc.card.v1"
id: "BPR-1552"
title: "The document-tabs key check honours the tour outline's reorder exception"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"regression on main 2026-10-16","regressed_by":"BPR-1532","governing_rfc":"RFC-BPR-0076","related_cards":["BPR-1532","BPR-1132"],"reservation":"docs/issues/_reservations/BPR-1552--BPR-1552--f28f517dbb38.json"}
acceptance: []
---

# BPR-1552: The document-tabs key check honours the tour outline's reorder exception

`qt_document_tabs_scale_{100,125,200}` fail on `main` since [[BPR-1532]]
landed: "assembled keys are unique" finds Alt+Shift+Up bound twice, on the
tour outline's `tourMoveUp` and the window's First change.

[[BPR-1532]] shares the pair on purpose. The tour editor's stop outline
reorders on Alt+Shift+Up and Alt+Shift+Down, widget-scoped, and the window
unbinds First and Last change while focus is in the outline
(`qt_change_end_keys`), so Qt never sees an ambiguous shortcut. That card
narrowed the two "no key is bound twice" checks in the window's scale rows to
skip exactly that pair, but the document-tabs course
([[BPR-1132]]) carries its own copy of the check, which was not narrowed.

The same triage covers `application_label_publication_recovery`, reported
failing on `main` at `32e9bcc0b`: whether it is a regression from the
2026-10-15/16 merges, an older failure or environmental.

## Acceptance

- AC1: `qt_document_tabs_scale_{100,125,200}` pass. Every duplicate-key
  check over the assembled window applies the one documented exception, in
  one place: only `tourMoveUp` and `tourMoveDown`, widget-scoped, on
  Alt+Shift+Up and Alt+Shift+Down. Any other key bound twice still fails, and
  a negative control shows it.
- AC2: `qt_change_end_keys`, `qt_workbench_window_scale_{100,125,200}` and
  `qt_profile_management_window` still pass; the shortcuts themselves are
  unchanged.
- AC3: `application_label_publication_recovery` is classified with evidence,
  and fixed if it is a regression from the day's merges.
- AC4: format, tidy and the documentation gate pass; one full `dev` ctest run
  on the result is recorded with any other failures on `main` named.
