---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ5"
title: "Every review request carries the brief and the editable bpr-pr-review skill"
status: "Backlog"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner decision D1, answer Q9 A)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0091 M5"}
---

# BPR-PENDING-AQ5: Every review request carries the brief and the editable bpr-pr-review skill

RFC-BPR-0091 M5 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)), owner decision D1 and answer Q9 A.

## Scope

- Context supplement version three: description, base and head, changed
  files, the diff named, threads, worktree facts, each with its bound.
- The built-in `bpr-pr-review` skill package with `metadata.bpr-version`,
  its text in every review request's brief.
- The profile's edited copy with its base version and digest, Save, Open in
  editor, Restore default, and the offer of a newer default in Settings ›
  Agents & context › Review skill.

## Acceptance criteria

- AC1: RFC AC08.
- AC2: RFC AC09.
