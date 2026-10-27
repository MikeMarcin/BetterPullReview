---
schema: "implement-rfc.card.v1"
id: "BPR-1783"
title: "A new tour stop starts with the keyboard in its title, not its body"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"defect","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0085"]}
acceptance: []
---

# BPR-1783: A new tour stop starts with the keyboard in its title

## Request

The owner, 2026-10-27, verbatim:

> adding a review stop should start with the focus on the stop title not the
> stop body

## Grounded current behavior

`review_tour_editor::show_stops` ends with `Body->setFocus()`, and the
Add stop path (`tour_editor_action::add_stop`) lands there, so a new stop
opens with the cursor in its explanation while its title is empty.

## Outcome

- **Add stop** (button, shortcut and outline insertion) opens the new stop with
  the keyboard in its **title** field, ready to type; Tab moves to the body.
- Opening an existing stop keeps today's behavior unless its title is empty,
  in which case the title takes the keyboard too.
- If the stop card has no editable title field, add one in the stop's header
  (the title is shown there today) rather than moving focus to a label.

## Acceptance (draft)

- A Qt test: after Add stop the focus widget is the new stop's title editor;
  typing names the stop; Tab reaches the body; an existing titled stop still
  opens on its body.
