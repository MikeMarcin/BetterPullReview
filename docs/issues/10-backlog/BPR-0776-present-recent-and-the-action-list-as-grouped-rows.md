---
schema: "implement-rfc.card.v1"
id: "BPR-0776"
title: "Present Recently viewed and the action list as grouped rows with one Filter menu"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0775"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A6","A7"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0776: Present Recently viewed and the action list as grouped rows with one Filter menu

## Outcome

Render Recently viewed and the grouped action list (Needs me, Waiting on others, Following) with one shared row delegate: identity, title, quiet progress or time, reason word amber only when the row needs me; Resume as a row action on hover, focus and the row menu; absent empty groups; one sentence with a Reviews & queue link when everything is empty; the six queue preferences as one Filter menu with chips, absent while nothing is listed, their store and reset unchanged.

## Acceptance

- Both lists use the same row anatomy from the existing core attention and recent projections; Qt invents no urgency.
- The Filter menu holds exactly the six typed preferences; chips reflect and clear the store; the menu is absent when nothing is listed.
- Empty groups are absent and the empty list is one sentence whose link opens Settings › Reviews & queue; no empty box renders.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
