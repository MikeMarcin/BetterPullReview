---
schema: "implement-rfc.card.v1"
id: "BPR-0771"
title: "Retire the locator, host picker, status banner and public-preview button from Accounts & hosts"
status: "In Progress"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A1","A4","A5"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0771: Retire the locator, host picker, status banner and public-preview button from Accounts & hosts

## Outcome

Stop parenting the session panel's locator group, host-profile picker row, public-preview strip and status banner into the Settings page. Route a pull-request load refusal to Home's bar notice with Copy details, a session failure to the server card's state, and any composed course that selected the retired objects to the bar, the card or the strip.

## Acceptance

- The Accounts page's widget and accessibility trees contain none of the retired objects in any connection state.
- A failed open shows one sentence and Copy details under the Home bar; a failed session shows in the card.
- Every existing composed course that named a retired selector passes against its new target; the retired widgets are deleted once unreferenced.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
