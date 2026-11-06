---
schema: "implement-rfc.card.v1"
id: "BPR-1878"
title: "The Tour tab lets an author pick an agent and says plainly what each control does"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: 'I see absolutely no way to pick an agent on the tour tab … This whole tab is a disaster.'"}
acceptance: []
---

# BPR-1878: The Tour tab lets an author pick an agent and says plainly what each control does

## Observed

The owner, 2026-11-05, authoring a tour for their own pull request. The Tour
tab shows:

- A banner: "Choose an available agent and wait for its current request.
  Manual editing is available." There is no agent picker anywhere on the tab,
  so the instruction cannot be followed.
- Step 1 "Problem" with a text box "What problem does this change solve?",
  then buttons **Recover explanation**, **Use selected description text** and
  **Next**, then steps 2 "Solution" and 3 "Stops", and "Saved locally. Nothing
  published." with **Preview and publish**.
- The owner could not tell what "Recover explanation" or "Use selected
  description text" do, and "Next" made no sense in that position.

## Change

Load the frontend-design skill and redesign the authoring surface so a first
time author understands it without reading docs:

- An agent picker on the tab (or a clear link to the one in the Agent tab)
  wherever agent help is offered; if no agent is available, say why and how to
  get one, in one line. Never instruct the author to choose something the tab
  does not show.
- Rename or replace "Recover explanation" and "Use selected description text"
  with labels that say the effect (e.g. draft from the PR description, draft
  with the agent), or drop them if redundant. Each control's tooltip says what
  it does.
- Step navigation that reads as a guided flow, or no "Next" at all if every
  step is visible; the current step and what is left are obvious.
- Keep manual editing first-class.

## Acceptance

- AC1. With an agent available, the author can choose it from the Tour tab's
  flow and start agent help; with none, the tab says why. Qt test.
- AC2. No control on the tab names an action the tab does not offer; every
  button label states its effect.
- AC3. The owner accepts the redesigned look (attended review, one question).
- AC4. `python3 scripts/verify_documentation.py --root .` passes.
