---
schema: "implement-rfc.card.v1"
id: "BPR-0772"
title: "Compose one server card per configured host with header, account row and rows"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0771"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A1","A4"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0772: Compose one server card per configured host with header, account row and rows

## Outcome

Add a `server_card` Qt widget that composes the existing account, connection, capability, rate-limit and diagnostic projections into header, account row, Access, Rate limit and Details, one card per configured host, with Connect on a non-active card dispatching the existing selection then connect intents. The Accounts page becomes the title, the sentence and the card column.

## Acceptance

- One card per configured host renders with the accepted header, account row and rows from unchanged projections.
- Host, login, transport and connection state each appear once on the page, checked by a text inventory in each state.
- Connect on a non-active card selects and connects through the existing intents; no widget gains an effect.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
