---
schema: "implement-rfc.card.v1"
id: "BPR-0783"
title: "Add Group, State headers, account chips and the Filter menu to Home"
status: "In Progress"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0776"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A7"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0783: Add Group, State headers, account chips and the Filter menu to Home

## Outcome

Put Group (None, Organization, Repository, Account), the State headers toggle, Filter and the refresh glyph in the strip before account and settings; render groups as neutral panels headed by the owner's or account's avatar from the existing avatar cache, with state bands nested inside when State headers is on and plain state-ordered rows when off; show a host only where it differs; place the account chip at the right of rows when the profile has more than one account, in a single-account group's header instead, and nowhere for a single-account profile; fold the surviving typed queue preferences into Filter as My role, Added by, Quiet items and Sort within a group, dropping Repository and Bucket as filters; persist Group, State headers, collapse and Filter per window.

## Acceptance

- Each Group value renders with State headers on and off against a two-account fixture, with avatars and chips placed per the account count and hosts shown only where they differ.
- Filter holds exactly the four surviving preferences over the existing store with reset unchanged; the strip shows the controls signed in and none signed out.
- Group, State headers, collapsed panels and Filter survive a window restart independently in two windows of one profile.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
