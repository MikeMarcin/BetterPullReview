---
schema: "implement-rfc.card.v1"
id: "BPR-0779"
title: "Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces"
status: "Ready"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033", "RFC-BPR-0034"]
depends_on: ["BPR-0773", "BPR-0774", "BPR-0778", "BPR-0783", "BPR-0790"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A8","B5","C6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0779: Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces

## Outcome

Extend the existing four-appearance, four-scale deterministic capture course to the Accounts page states, the Home states, the Appearance page states with every tile, and the Profiles sheet states; walk the keyboard order and VoiceOver names on macOS; prepare one attended candidate per surface, including the close-and-reopen course, through the shared attended-review workflow with absolute paths and one question per criterion.

## Acceptance

- Captures exist for every named state at 100, 125, 150 and 200 percent in the four appearances with the Home column at most 720 points.
- Keyboard and VoiceOver walks reach every control with an accessible name; the profile row state is its description.
- The owner's macOS verdicts are bound to the executable digest; unobserved Windows and Linux items are recorded as not run.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
