---
schema: "implement-rfc.card.v1"
id: "BPR-0775"
title: "Lead Home with the bar and the first-launch card"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A5"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0775: Lead Home with the bar and the first-launch card

## Outcome

Make the bar an address field alone in Home's first row (label, placeholder, ⌘L, Return, focused on arrival); remove the landing's focus-only buttons; make the signed-out landing the first-launch card with Sign in filled and Add enterprise server bordered and relabel the bar for public reading; show the strip with Home, account and settings only while signed out; seat a load refusal under the bar with Copy details; keep the pull-request stack's empty page off Home; keep the over-a-review return as one row; carry the session line in the status bar on Home.

## Acceptance

- Signed in, the bar is the first row, labelled, focused; the landing has no focus-only buttons; a refusal sits under the bar.
- First launch and signed out show only the strip, the relabelled bar, the sign-in card with its two ways in, and the status bar line.
- The status bar never reads that no pull request is open while Home is shown; Home over a review keeps its one return row.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
