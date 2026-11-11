---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "Save policy and the worktree write port"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: ["BPR-2020"]
metadata: {"criteria": ["AC02", "AC03", "AC06"], "milestone_plan": {"id": "local-draft-authoring-v1", "path": "docs/rfcs/assets/local-draft-authoring-v1/milestone-plan.json"}}
---

# Save policy and the worktree write port

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0001](../RFC-DRAFT-0001-edit-and-save-files-in-a-local-pr-draft.md), criteria `AC02`, `AC03`, `AC06`.

The core save decision (the identity comparison, the editable-file policy, each refusal reason and the typed outcome) and the application's narrow write port: one named regular file beneath a local draft's worktree root, no link followed, a complete old or new file after any failure. The existing workspace adapter stays read-only, and `architecture_boundaries` holds the port to its named callers.

## Intended verification

Core tests over the save decision and every not-editable reason. Host tests against a real temporary worktree: byte-exact writes across line endings and a missing final newline, a file changed underneath, a link, a path outside the root, and an interrupted write. The boundary test fails when a widget or another adapter reaches the port.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
