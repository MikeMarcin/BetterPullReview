---
schema: "implement-rfc.card.v1"
id: "BPR-0923"
title: "Clarify or replace the Show review threads action"
status: "Ready"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Clarify or replace the Show review threads action

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-06. Earlier RFC references retain their governing contracts.

## Owner feedback

“‘Show review threads’ is a confusing button that doesn't do what I would
expect it to do, I'm not sure what it does or what I would expect.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Trace the button's actual intent and visible effect, including fetching,
revealing, filtering and selection, before choosing a label or deciding whether
a button is needed. The owner has not specified an expected behavior; record
that uncertainty rather than inventing it. Coordinate drawer redesign
[[BPR-0920]] and empty Discuss [[BPR-0926]].

Starting point: `src/adapters/qt/src/github_conversation_panel.cpp`.

## Acceptance

- Document the action’s actual behavior and why its current label does not make that effect apparent.
- Propose a clear label/placement or remove the redundant gesture with an explicit replacement behavior; show before/after interaction examples.
- The resulting interaction distinguishes loading, no threads, filtered threads and failure, with accessible action/state text and no silent no-op.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
