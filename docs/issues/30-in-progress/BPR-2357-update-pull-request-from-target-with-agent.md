---
schema: "implement-rfc.card.v1"
id: "BPR-2357"
title: "Update an authored pull request from its target with an agent"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2357--BPR-2357--f1fd050f3098.json","related_cards":["BPR-2327"],"related_rfcs":["RFC-BPR-0085","RFC-BPR-0101"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2357: Update an authored pull request from its target with an agent

The owner wants one button that asks the agent to fetch the pull request's
target branch, merge it into the pull request's branch, resolve conflicts, and
push the update. The guidance is editable in Settings → Prompts.

## Acceptance

- AC1: An open pull request's authenticated author can start the action. The
  Status drawer suggests the same action when GitHub reports a merge-blocking
  conflict. The action is absent for other accounts, signed-out and closed
  reviews, and a fresh authority check guards each press.
- AC2: A single press sends one agent turn about the accepted pull request,
  with its actual target repository and ref and its actual head repository,
  ref and commit. A fork keeps its own push destination. The turn uses the
  existing agent setup, permissions and workspace path.
- AC3: Settings → Prompts shows the shipped guidance and lets the user
  supplement or replace it. Saved guidance reaches the agent while the
  application provides the current destination independently.
- AC4: Local fixtures exercise the composed action and prove non-author
  refusal, conflict suggestion and exact fork/target routing without spending
  agent usage or updating a live pull request.
