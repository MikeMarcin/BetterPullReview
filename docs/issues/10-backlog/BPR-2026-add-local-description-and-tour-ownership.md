---
schema: "implement-rfc.card.v1"
id: "BPR-2026"
title: "Add local description and tour ownership"
status: "Backlog"
rfcs: ["RFC-BPR-0110"]
metadata: {"criteria":["AC01","AC03","AC04"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0011","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0038-add-local-description-and-tour-ownership.md"}
depends_on: ["BPR-2016", "BPR-2017", "BPR-2022", "BPR-2025"]
---

# Add local description and tour ownership

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0110](../../rfcs/RFC-BPR-0110-local-tours-and-description.md), criteria `AC01`, `AC03`, `AC04`.

Local tour/material ownership seam, description authority, the local-draft session scope and candidate workspace under the adopted native-session contract, and the local-authoring prompt purposes in Settings and the sharing format; preserve current PR-owned tours and their sessions.

## Intended verification

Author before any PR exists, refresh dirty candidates, reject stale proposals and resume a conversation after restart with no account. Accept a refresh and observe a new session on the successor candidate with the earlier conversation still readable. Change an unselected live helper while a proposed stop's candidate lines remain valid: neither the candidate workspace nor the tool-free fallback may supply that live edit. Verify the limited-context or unavailable-agent outcome when the candidate workspace cannot be used.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).
