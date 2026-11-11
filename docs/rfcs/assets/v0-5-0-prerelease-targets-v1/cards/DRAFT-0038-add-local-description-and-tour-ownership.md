---
schema: "implement-rfc.card.v1"
id: "DRAFT-0038"
title: "Add local description and tour ownership"
status: "Backlog"
rfcs: ["RFC-DRAFT-0013"]
depends_on: ["DRAFT-0028", "DRAFT-0029", "DRAFT-0034", "DRAFT-0037"]
metadata: {"criteria": ["AC01", "AC03", "AC04"], "milestone_plan": {"id": "v0-5-0-prerelease-targets-v1", "path": "docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"}, "absorbed_from": "local PR drafts pack v1, DRAFT-0011"}
---

# Add local description and tour ownership

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0013](../RFC-DRAFT-0013-local-tours-and-description.md), criteria `AC01`, `AC03`, `AC04`.

Local tour/material ownership seam, description authority, the local-draft session scope and candidate workspace under the adopted native-session contract, and the local-authoring prompt purposes in Settings and the sharing format; preserve current PR-owned tours and their sessions.

## Intended verification

Author before any PR exists, refresh dirty candidates, reject stale proposals and resume a conversation after restart with no account. Accept a refresh and observe a new session on the successor candidate with the earlier conversation still readable. Change an unselected live helper while a proposed stop's candidate lines remain valid: neither the candidate workspace nor the tool-free fallback may supply that live edit. Verify the limited-context or unavailable-agent outcome when the candidate workspace cannot be used.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
