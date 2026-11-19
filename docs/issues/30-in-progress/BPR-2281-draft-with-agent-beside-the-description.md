---
schema: "implement-rfc.card.v1"
id: "BPR-2281"
title: "Draft with agent sits beside the description in Author, not in the Ask menu"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2280"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"change","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2281--BPR-2281--ff2554fb5b87.json","decided_by":"owner, 2026-11-19","related_cards":["BPR-2280","BPR-2274"]}
---

# BPR-2281: Draft with agent sits beside the description

## Owner (2026-11-19, on BPR-2280)

"I wouldn't put draft description in the Ask menu, I would put it near the
description field in Author."

## Change

- "Draft a description" is no longer in the pull request's Ask menu or among
  the chips. The shipped prompt stays in Settings › Prompts, where its text
  is edited.
- Author's Workflow tab offers **Draft with agent** beside the description's
  Edit/Write. It runs that prompt with the configured agent and puts the
  answer in the description editor as one undoable edit. While it runs, and
  if it fails, the line beside the field says so. Nothing is sent until Save.
- A typed or otherwise sent description request still answers in the Agent
  tab with Use as description ([[BPR-2280]]).

## Acceptance

- AC1. The shipped prompt is not a quick ask and not a chip; it is still a
  description prompt in the library (core test).
- AC2. Public path with the fixture agent: Draft with agent fills the
  editor with the description, says it is running while it runs, sends
  nothing until Save; Save writes once.
- AC3. Qt test: the button, its running and failure states, and the fill as
  one undoable edit.
- AC4. architecture_boundaries, coding-format-check and the documentation
  gate pass.
