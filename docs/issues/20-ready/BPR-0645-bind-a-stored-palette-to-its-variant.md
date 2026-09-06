---
schema: "implement-rfc.card.v1"
id: "BPR-0645"
title: "Bind a stored palette to the variant it customizes"
status: "Ready"
rfcs: ["RFC-BPR-0018"]
depends_on: []
priority: "P1"
---

# Bind a stored palette to the variant it customizes

Owner direction, 2026-09-06: the workbench renders half light and half dark on
a dark macOS desktop and is unusable; this is the first slice of the
owner-directed UX track and the explicit post-pack frontier exception for
RFC-BPR-0018 recorded in [private-alpha.md](../../development/private-alpha.md).

Observed: `appearance_settings` holds `mode=system` and a stored palette that
is byte-for-byte the built-in light palette (46 of 46 roles equal). Under
`qt::appearance_profile`'s constructor the stored palette is read as overrides
against the built-in profile of the variant the *current* observation
resolves, so on a dark desktop all 46 roles differ and pin light colours over
the dark built-in. Widgets painted from the inherited `QPalette` render light
while the native toolbar, title bar and text fields follow the dark desktop.
This is BPR-0216's defect reappearing for stored records.

## Scope

A stored palette customizes exactly one variant. Persist that variant with the
record (`palette_variant`, schema format 2, migration deriving the variant of
existing rows from their mode under an empty observation, which is what the
old reading assumed). The Qt profile computes overrides only against the
built-in palette of the stored variant and applies them only while that
variant is resolved; any other resolved variant renders its built-in palette
untouched. Palette adoption records the variant in effect. Owned paths:
`src/application` appearance ports and host, `src/adapters/sqlite` appearance
settings database/store, `src/adapters/qt` appearance profile, their focused
tests, and the appearance architecture notes.

## Acceptance

- A record whose palette equals a built-in palette produces zero overrides in
  every resolved variant; a light customization never colours the dark variant.
- Format-1 records migrate with the derived variant and unchanged typography.
- `qt_appearance_profile`, `application_appearance_host`,
  `sqlite_appearance_settings_storage`, `qt_workbench_window_scale_100` and
  `qt_composed_github_scale_100` pass; the architecture boundary gate passes.
- The tests-disabled Mac app on a dark desktop renders toolbar, header, fields
  and panels from one dark palette (attended capture).
