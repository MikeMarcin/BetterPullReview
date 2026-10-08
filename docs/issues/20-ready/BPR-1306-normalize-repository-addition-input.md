---
schema: "implement-rfc.card.v1"
id: "BPR-1306"
title: "Normalize repository addition input for an exact displayed destination"
status: "Ready"
rfcs: ["RFC-BPR-0062"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# Normalize repository addition input for an exact displayed destination

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Host accepts owner/name and canonical HTTPS repository roots with optional trailing slash; malformed paths, ports, userinfo, query and fragments refuse with typed reasons.
- C2: Normalized input cannot change accepted host/account/session authority or configure a server; core admission decides from bounded authority facts.
