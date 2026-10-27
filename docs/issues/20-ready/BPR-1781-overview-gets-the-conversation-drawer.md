---
schema: "implement-rfc.card.v1"
id: "BPR-1781"
title: "Overview gets the conversation drawer: Comments, Notes, Tour and Agent, with the pull request's conversation on its Comments tab"
status: "Ready"
rfcs: []
depends_on: ["BPR-1779"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1781--BPR-1781--4d335607ec69.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0085","RFC-BPR-0039","RFC-BPR-0040"],"related_cards":["BPR-1588"]}
acceptance: []
---

# BPR-1781: Overview gets the conversation drawer

## Request

The owner, 2026-10-27, verbatim:

> Why isn't the tour tab on overview for an authored PR, or possibly any PR

and, on the proposal of a Tour tab: "Yeah do that". Then, widening it:

> Honestly comments, notes and agents probably also make sense on overview.
> Probably makes more sense than how we did the poll request comments tucked
> away as a mode in the top of file"s comments.

Revision r2 (2026-10-27) widens the card from a Tour tab to the whole
drawer. The Tour tab outcome below is kept as one of the four tabs.

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
- The pull request's own conversation (pull-request comments and reviews) is
  a mode of Review's Comments tab: a **Pull request | File** switch at its
  top (`github_conversation_panel.cpp`, `ScopeWholePr`,
  `cards_scope::whole_pull_request`). Overview, where a reviewer reads the
  description and decides how to start, shows none of it; notes and the
  agent are not on Overview either.

## Outcome

Overview's right drawer holds the same conversation tabs Review does,
**Comments, Notes, Tour, Agent**, beside Status and Commits (whether Status
and Commits stay tabs of the same drawer or a separate column is the look's
question; one drawer with six tabs is the starting proposal).

- **Comments on Overview is the pull request's conversation:** pull-request
  comments and reviews, shown comments-first as [[BPR-1779]] defines, with
  the composer for a new pull-request comment. It moves here from Review.
- **Review's Comments tab is about code:** the threads of the file shown, at
  their lines. The **Pull request | File** switch goes; in its place one quiet
  link, "Pull request conversation", opens Overview's Comments tab. (Keeping a
  whole-pull-request mode in Review is the alternative for the look.)
- **Notes on Overview:** the pull-request notes (not on a line), with New
  note; line notes stay with the code in Review, and the list can show them
  all with their file and line.
- **Agent on Overview:** the same Agent tab and the same conversation per
  pull request and agent ([[BPR-1753]]); a question asked on Overview has the
  pull request as its scope, not a selection.
- **Tour on Overview** as below.
- One instance of each surface's state: a note, a draft comment or an agent
  turn is the same object whichever window part shows it; switching between
  Overview and Review keeps each tab's place.

### The Tour tab

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

- Overview shows Comments, Notes, Tour and Agent; Overview's Comments holds
  the pull-request conversation with its composer; Review's Comments has no
  Pull request mode and links to Overview's; Notes and Agent show the same
  objects in both places (Qt and composed tests).
- Tour: the tab exists for an author (with and without a draft) and for a
  published tour, and not otherwise; Continue in Review and Start tour open
  Review's Tour tab at the right stop; nothing is published or changed.
- Offscreen renders, light and dark: author with a draft, author with none,
  reader of a published tour.
- The owner accepts the look on a frozen build (attended review).
