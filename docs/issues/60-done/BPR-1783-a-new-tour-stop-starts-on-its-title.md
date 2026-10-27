---
schema: "implement-rfc.card.v1"
id: "BPR-1783"
title: "A new tour stop starts with the keyboard in its title, not its body"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"defect","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0085"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "20ea2083468a5bb323b72af77b9cb06d69dec1f0", "evidence": {"kind": "command", "detail": "application_review_tour_composition_public_path seed_route: after the window's Add stop action the new untitled stop's title has focus; typing names it; Tab from the title reaches the body; after saving, show_stops on the titled stop focuses the body. All 19 application_review_tour*, profile_tour and qt_workbench_window_scale_100 tests pass."}}]
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

## Acceptance

- A Qt test: after Add stop the focus widget is the new stop's title editor;
  typing names the stop; Tab reaches the body; an existing titled stop still
  opens on its body.

## Built

- `review_tour_editor::show_stops` puts the keyboard in the title when the
  selected stop has none, otherwise in the body as before.
- `present_stops`: when the stop being edited changes to one with no title
  and the keyboard was in the stop's fields (including across the route
  rebuild that Add stop causes), the title takes it. The title field already
  existed in the stop's header, so none was added.

## Not verified

- The owner's look on a real display (deferred with the other looks).
- Insert after and outline insertion share the same presentation path and
  were not exercised separately.
