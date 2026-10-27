---
schema: "implement-rfc.card.v1"
id: "BPR-1781"
title: "A Tour tab on Overview: the author's draft and any published tour, at a glance"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1781--BPR-1781--4d335607ec69.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0085","RFC-BPR-0039","RFC-BPR-0040"],"related_cards":["BPR-1588"]}
acceptance: []
---

# BPR-1781: A Tour tab on Overview

## Request

The owner, 2026-10-27, verbatim:

> Why isn't the tour tab on overview for an authored PR, or possibly any PR

and, on the proposal below: "Yeah do that".

## Grounded current behavior

- The Tour tab is the third tab of Review's right drawer (Comments, Notes,
  Tour, Agent; `workbench_window.cpp`, `tray_tour_tab`), by RFC-BPR-0085 D4:
  a tour is written and followed beside the code, whose lines wash amber.
- Overview's right side is its own drawer, **Status | Commits**
  (`overview_status_drawer.cpp`). The only tour control there is
  [[BPR-1588]]'s small **Prepare review tour** icon in that drawer's tab row
  (`overviewPrepareTour`), following `PrepareTourAction`, which switches to
  Review and opens the Tour tab; it is disabled until the composition
  enables it.
- So Overview says nothing about whether an authored pull request has a tour
  draft or how far along it is, and a reader sees no sign of a published
  tour.

## Outcome

A **Tour** tab in Overview's drawer, beside Status and Commits:

- **My pull request:** the brief (Problem, Solution) and the stops in order,
  each with its readiness (Ready, Editing, Needs attention, Suggested) as the
  route shows it; whether the tour is a private draft or published; and
  **Continue in Review**, which opens Review's Tour tab at the current stop.
  With no tour yet, **Prepare review tour** as a plain labelled action (the
  icon goes, or stays only as a shortcut).
- **Any pull request with a published tour:** the brief and the stop titles,
  read-only, with **Start tour**, which opens Review at the first stop as
  following a tour does today.
- **No tour, and not the author:** no tab.
- Writing and following stay in Review beside the code; this tab presents and
  routes, and performs no effect (the composition decides every gesture).
- Uses the drawer kit (RFC-BPR-0085) and the shared buttons (RFC-BPR-0087);
  load `frontend-design:frontend-design` first.

## Acceptance (draft)

- Qt tests: the tab exists for an author (with and without a draft) and for a
  published tour, and not otherwise; Continue in Review and Start tour open
  Review's Tour tab at the right stop; nothing is published or changed.
- Offscreen renders, light and dark: author with a draft, author with none,
  reader of a published tour.
- The owner accepts the look on a frozen build (attended review).
