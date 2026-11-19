---
schema: "implement-rfc.card.v1"
id: "BPR-2274"
title: "A tour is only its stops: Problem and Solution are removed everywhere; a shipped prompt drafts the pull request description"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"change","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2274--BPR-2274--fddc088f0e78.json","decided_by":"owner, 2026-11-18"}
---

# BPR-2274: A tour is only its stops

## Owner decision (2026-11-18)

"I agree with the no fixed sections. I think the tour just becomes the tour
stops. That streamlines it ... gives back some space and complexity on the
tour tab." "Tours right now have a message saying problem and solution in
description ... get rid of that language and just have the tour focused only
on stops." "It's also part of the cli."

## Change

Remove the tour's Problem and Solution (the tour brief) from core, application,
Qt, publication, CLI and the pr-tour skill. Publish posts the stops only and
never writes or edits the pull request description. A new shipped prompt,
"Draft a description", drafts the pull request description from the diff and
the tour's stops when there is one.

## Acceptance

- AC1. No Problem/Solution field, step, editor, chat, hook, CLI command or
  publication text remains outside migration code and its tests.
- AC2. Publish never touches the pull request description; the "problem and
  solution are in the pull request description" line is gone.
- AC3. Old drafts with Problem/Solution migrate into the first stop with no
  text lost (test).
- AC4. "Draft a description" is a shipped prompt that returns a description
  from the diff, with and without a tour (fixture agent test).
- AC5. Focused tests, architecture_boundaries, coding-format-check and the
  documentation gate pass.
