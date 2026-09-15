---
schema: "implement-rfc.card.v1"
id: "BPR-0773"
title: "Put the device sign-in inside the account row as one gesture"
status: "Ready"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0772"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A2"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0773: Put the device sign-in inside the account row as one gesture

## Outcome

Present the existing `device_authorization_panel` state machine as the account row's signing-in state: the address sentence, the selectable code in the code font at 1.6×, Open browser filled, Copy code, the validity, one status sentence and Cancel; denied, expired, unavailable and cancelled states in the same row with one retry; the manual-token path in the same row where the device flow is unsupported.

## Acceptance

- One Sign in gesture shows the code and browser controls inside the card with no disclosure or scroll.
- Cancel, expiry, denial, unavailable and manual-token cases render in the row; no disabled button or truncated field is shown.
- The device public-path course passes unchanged in what it proves about authorization; only presentation moved.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
