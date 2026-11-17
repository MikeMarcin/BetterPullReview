---
schema: "implement-rfc.card.v1"
id: "BPR-2248"
title: "Ask what changed in a selection of commits from the Commits tab"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2248--BPR-2248--f74f9df334a2.json","origin":"owner request, 2026-11-17"}
---

# Ask what changed in a selection of commits from the Commits tab

## Request

The owner, 2026-11-17: "It'd be cool to have the ability to ask what changed
in a selection of commits from the commit tab".

## Shape to settle

- Select one or more commits in the Commits tab (contiguous or not) and
  choose an Ask action, for example "Ask what changed in these commits".
- The agent receives the diff of exactly the selected commits. Merge commits
  that bring in the base branch are left out or flagged, the same rule the
  "What changed since my review" fix uses (worked on in parallel, 2026-11-17).
- The answer is a report like "What changed since my review", with Draft
  comment available.
- Commits missing from the local clone are fetched by the host, as in
  BPR-2246.

## Acceptance

- With commits selected, the Ask action is offered. It runs on exactly those
  commits, and the request names them.
- Base-merge commits in the selection are handled as described above.
- The action works on a shallow clone.
- The UI work loads frontend-design and gets an attended look with the
  Fixture agent.
