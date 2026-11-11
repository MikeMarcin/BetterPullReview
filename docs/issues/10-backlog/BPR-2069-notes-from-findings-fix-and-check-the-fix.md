---
schema: "implement-rfc.card.v1"
id: "BPR-2069"
title: "Self-review with its own prompt, notes from findings, Fix, and Check the fix"
status: "Backlog"
rfcs: ["RFC-BPR-0115"]
metadata: {"criteria":["AC01","AC05","AC06","AC13"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0008-notes-from-findings-fix-and-check-the-fix.md"}
depends_on: ["BPR-2068", "BPR-2016"]
---

# Self-review with its own prompt, notes from findings, Fix, and Check the fix

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0115](../../rfcs/RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md), criteria `AC01`, `AC05`, `AC06`, `AC13`.

Review my change as a read-only run on the candidate, briefed by the Self-review built-in prompt: its own entry in Settings › Agents & context › Built-in prompts with editor, default, version and Reset, the stable purpose `local-draft.self-review`, and its own item in the sharing format. Keep as note and Keep all as notes on a local draft's finding cards; Fix on a note and on a finding card; the note's record of the run and resulting candidate; and Check the fix through the existing `evaluate_proposed_fix` entry point on the successor candidate. Load `frontend-design:frontend-design` before the Qt work.

## Intended verification

Prompt tests in the shape of the existing writing-hook ones: an edit reaches the next self-review brief and no other prompt, edits to the review skill, Suggested comments and whole-change discussion prompts leave it alone, Reset restores the default, and it round-trips through export and import. The Draft all tests' shapes applied to Keep all as notes; the composed path from a fixture agent's findings to notes, a Fix run, a kept change, a successor candidate and a Check the fix answer shown as a claim.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).
