---
schema: "implement-rfc.card.v1"
id: "BPR-0775"
title: "Lead Home with the bar and seat refusals beside it"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A5","A7"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0775: Lead Home with the bar and seat refusals beside it

## Outcome

Give Home its 720-point column; make the bar the first object with its heading inside, the placeholder and filled Open, focused on arrival; remove the landing's focus-only buttons and the signed-out card's second button; keep the over-a-review return as one row; show a load refusal under the bar; keep the pull-request stack's empty page off Home; carry the session line in the status bar on Home.

## Acceptance

- Signed in with nothing open, the bar is first, headed, focused; the landing has no focus-only buttons.
- Signed out, Home is the sign-in card with one filled button, the bar headed for public reading, and the status bar with the session line.
- The status bar never reads that no pull request is open while Home is shown; Home over a review keeps its one return row.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
