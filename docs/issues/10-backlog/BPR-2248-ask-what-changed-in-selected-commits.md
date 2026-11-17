---
schema: "implement-rfc.card.v1"
id: "BPR-2248"
title: "Agent actions on selected commits in the Commits tab"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2248--BPR-2248--f74f9df334a2.json","origin":"owner request, 2026-11-17"}
---

# Agent actions on selected commits in the Commits tab

## Request

The owner, 2026-11-17: "It'd be cool to have the ability to ask what changed
in a selection of commits from the commit tab". Then: "Or review selected
commits or something. Decide what would be actually useful to get agentic
feedback on in the commits tab".

## Decision (coordinator, 2026-11-17)

The Commits tab's selection menu gets two agent actions. They have the same
scope, so it does not grow into a menu of near-duplicates.

1. **Explain these commits** (Answer). A walk-through of what the selected
   commits change and why, read against their messages. It flags commits whose
   diff does not match their message. This is what a reviewer wants when
   commits arrive on a pull request they have already reviewed.
2. **Review these commits** (Findings report). The review prompt, limited to
   the selected commits' diff, so a reviewer can review new work without
   re-reading the whole pull request.

Both are ordinary shipped prompts that can be placed on a new "Commits
selection" surface, so a user can add their own prompts there too.

Rejected options:
- One action per commit. Explain covers a single selected commit.
- "Split or squash advice". That concerns the author's history, not the
  reviewer.
- A separate "what changed" action. It is Explain.

## Shape

- Select one or more commits in the Commits tab (contiguous or not) and
  choose Explain these commits or Review these commits.
- The agent receives the diff of exactly the selected commits. Merge commits
  that bring in the base branch are left out or flagged, the same rule the
  "What changed since my review" fix uses (worked on in parallel, 2026-11-17).
- Explain answers in the Agent tab. Review produces a findings report with
  Draft comment.
- Commits missing from the local clone are fetched by the host, as in
  BPR-2246.

## Acceptance

- With commits selected, both actions are offered. Each runs on exactly those
  commits, and the request names them.
- Base-merge commits in the selection are handled as described above.
- The action works on a shallow clone.
- The UI work loads frontend-design and gets an attended look with the
  Fixture agent.
