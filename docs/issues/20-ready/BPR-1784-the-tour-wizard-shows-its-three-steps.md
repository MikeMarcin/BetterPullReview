---
schema: "implement-rfc.card.v1"
id: "BPR-1784"
title: "The tour wizard shows its three steps: Problem, Solution, Stops"
status: "Ready"
rfcs: []
depends_on: ["BPR-1783"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0085","RFC-BPR-0092"],"related_cards":["BPR-1776","BPR-1781"]}
acceptance: []
---

# BPR-1784: The tour wizard shows its three steps

## Request

The owner, 2026-10-27, verbatim:

> the tour wizard should borrow some inspiration from the agent setup
> wizard... make it more clear that the steps are fill out problem, fill out
> solution, fill out stops.

## Grounded current behavior

The Tour tab (RFC-BPR-0085 D4) is a route: the brief (Problem, Solution) as
one compact card that opens in place, then the stops on an amber route line.
Nothing says there is an order or how far along the tour is. The agent setup
(RFC-BPR-0092, `agent_setup_view.cpp`; design frames
`docs/design/BPR-1747-agent-setup-v1/`) is a numbered list of steps: a
circle per step (number, green tick when done, amber when it needs
attention), the current step open with its controls, later steps quiet, done
steps folded to one line with Change.

## Outcome

- The tour reads as three numbered steps in the agent setup's visual
  language: **1 Problem**, **2 Solution**, **3 Stops**. Each shows its state
  (to do, current, done with a tick, needs attention), the current one open,
  done ones folded to one line with what was written and Change.
- Step 3 holds the existing stops route (numbered amber stops, readiness
  rings), so stop numbers and step numbers must not be confused (e.g. the step
  circle is neutral/green, stops stay amber).
- Moving on is explicit (Next, or ⌥↩ as the stops use) and never blocks
  editing an earlier step.
- Publish stays at the foot once all three are done enough to publish
  (existing readiness rules); [[BPR-1782]]'s feedback applies there.
- Works in the drawer and on the expanded tour page; [[BPR-1781]] may also
  show the tour on Overview, which reads the same step states.
- Load `frontend-design:frontend-design` first; design frames before code
  (the agent setup's frames are the model).

## Acceptance (draft)

- Qt tests: the three steps and their states follow the draft; done steps
  fold and Change reopens; focus lands in the current step's first field
  (Problem's text, Solution's text, a new stop's title per [[BPR-1783]]).
- Offscreen renders light and dark: empty tour, Problem done, both done with
  stops in progress, ready to publish.
- The owner accepts the look on a frozen build (attended review).
