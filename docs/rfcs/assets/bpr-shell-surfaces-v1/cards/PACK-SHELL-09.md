---
schema: "implement-rfc.card.v1"
id: "PACK-SHELL-09"
title: "Redesign the chooser as rows with in-place rename and create"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: ["PACK-SHELL-08"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"profile-chooser","acceptance":["B4"],"alias_requires_live_allocation":true}
---

# PACK-SHELL-09: Redesign the chooser as rows with in-place rename and create

## Outcome

Rebuild `profile_management_window` as the Profiles sheet: rows with avatar, name and state; current profile selected on open; Open or New window as the filled footer action; Rename and New profile editing in place with Save and Cancel; one failure notice with Try again only after a failure; no permanent name field, status sentence or Reload button; the same dispatch and projection.

## Acceptance

- The component course proves rows, selection on open, Return and double-click, in-place rename and create, and text kept on Escape and failure.
- The failure notice appears only after a failure and its Try again reloads; the retired controls are absent from the widget and accessibility trees.
- Accessible names, row state descriptions and the declared tab order pass the existing focus checks.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
