---
schema: "implement-rfc.card.v1"
id: "BPR-0776"
title: "Render the list as state panels with one row anatomy"
status: "In Progress"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0775"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0776: Render the list as state panels with one row anatomy

## Outcome

Add a `home_panel` widget (bordered, rail in the state colour, tinted sticky header with name, count and chevron, collapsible by pointer and keyboard with a one-line summary when collapsed) and a `home_row_delegate` (identity, title, why sentence, reason word in the state colour, time and progress) shared by the queue and Recently viewed models; rows always in state order from the core attention projection; absent empty states; the one-sentence all-empty state with its Reviews & queue link; Resume as a row action; Recently viewed as the last panel.

## Acceptance

- Rows are in state order with one anatomy for both models; colour appears only on the reason word and the rail or mark.
- Each state panel collapses by chevron, Space, Return, Left and Right, keeps its count and summary, and its header sticks under scroll.
- Empty states are absent, the all-empty list is one sentence whose link opens Settings › Reviews & queue, and no empty box renders.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
