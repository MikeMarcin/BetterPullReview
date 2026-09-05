---
schema: "implement-rfc.card.v1"
id: "BPR-0005"
title: "Implement the native Qt presentation adapter"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: ["BPR-0004"]
completed: "2026-07-29"
---

# Implement the native Qt presentation adapter

Build the native empty-state workbench and diagnostics surface. Translate a
real `QAction` into a semantic intent, apply command/status projections
incrementally, and cover event translation, accessibility, focus, and action
state without direct effect calls.

## Result

- Added a Qt Widgets `QMainWindow` with a native toolbar, empty state, build
  facts, status surface, and structured diagnostic presentation.
- Bound the Refresh action and standard refresh shortcut to one semantic
  `refresh_requested_intent`.
- Applied command, status, and diagnostic projections independently without a
  model/window reset.
- Kept diagnostic wording at the presentation boundary rather than exposing
  internal diagnostic detail.
- Added accessible names/descriptions for the window state, toolbar, tool
  button, status, diagnostics, and build facts.
- Confirmed the Qt adapter sources contain no GitHub, SQLite, network, process,
  listener, or direct effect calls.

## Evidence

`qt_foundation_window` passes under the offscreen Qt platform and directly
checks QAction translation, keyboard access, accessibility, visible build
facts, independent projection updates, presentation-owned diagnostics, and
zero full resets. The full `dev` suite passes; the `core-only` suite continues
to configure and pass without building Qt sources.
