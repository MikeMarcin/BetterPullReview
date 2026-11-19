---
schema: "implement-rfc.card.v1"
id: "BPR-2280"
title: "Draft a description writes a pull request description, and the Author tab can edit the description"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2280--BPR-2280--3417485e427c.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2274","BPR-1774","BPR-2196"]}
---

# BPR-2280: Draft a description writes a pull request description

## Owner (attended review, 2026-11-19)

Rejecting BPR-2274's "Draft a description" look: "That's not working at all.
It looks like it tried to make a comment not draft a PR description. Also
seems like we can't edit the PR description from Author tab which seems like a
gap."

## Change

- "Draft a description" asks for a description: its result is a pull request
  description (title-free Markdown: summary, what changed, how to test), the
  answer is presented as a description and never offered as a review comment,
  and the fixture agent answers it with a description.
- Author: the author reads and edits the pull request description in the
  Author drawer's Workflow tab and saves it to GitHub through the existing
  description mutation ([[BPR-1774]]). The draft's answer offers "Use as
  description", which fills that editor; nothing is published until Save.
  A description changed on GitHub since it was read is refused and offered
  for reload; it is never overwritten.

## Acceptance

- AC1. The shipped prompt's ask kind is a description; such a question is
  sent without the comment-label guidance and its answer offers Use as
  description, not Draft (core/application tests).
- AC2. Fixture-agent course: Draft a description returns a description with
  summary, what changed and how to test; no Draft comment; Use as
  description fills the Author editor; Save sends the description mutation
  through the fixture transport; a stale description is refused.
- AC3. Qt test for the Author description editor.
- AC4. architecture_boundaries, coding-format-check and the documentation
  gate pass.
