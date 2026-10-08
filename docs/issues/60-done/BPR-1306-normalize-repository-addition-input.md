---
schema: "implement-rfc.card.v1"
id: "BPR-1306"
title: "Normalize repository addition input for an exact displayed destination"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1: Host accepts owner/name and canonical HTTPS repository roots with optional trailing slash; malformed paths, ports, userinfo, query and fragments refuse with typed reasons.","outcome":"pass","revision":"300f4f94bf2b92c685743d9e56d07591aea1b6f6+sha256:848bce079aa6358e2ebf4b2b68c98432d84affa0fda250a68e911ef1498ef9eb","evidence":{"kind":"command","detail":"application_setup_destination passed in dev; core_dependency_boundary, core_boundary_contract and architecture_boundaries passed. Parser tests cover safe host preservation, unsafe/non-root inputs and bounds; core tests cover accepted/stale authority, window, unavailable/busy/duplicate/capacity. Formatting and warnings-as-errors tidy passed.","manifest":"artifacts/four-drafts-r1/BPR-1306-candidate.json"}},{"criterion":"C2: Normalized input cannot change accepted host/account/session authority or configure a server; core admission decides from bounded authority facts.","outcome":"pass","revision":"300f4f94bf2b92c685743d9e56d07591aea1b6f6+sha256:848bce079aa6358e2ebf4b2b68c98432d84affa0fda250a68e911ef1498ef9eb","evidence":{"kind":"command","detail":"application_setup_destination passed in dev; core_dependency_boundary, core_boundary_contract and architecture_boundaries passed. Parser tests cover safe host preservation, unsafe/non-root inputs and bounds; core tests cover accepted/stale authority, window, unavailable/busy/duplicate/capacity. Formatting and warnings-as-errors tidy passed.","manifest":"artifacts/four-drafts-r1/BPR-1306-candidate.json"}}]
---

# Normalize repository addition input for an exact displayed destination

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Host accepts owner/name and canonical HTTPS repository roots with optional trailing slash; malformed paths, ports, userinfo, query and fragments refuse with typed reasons.
- C2: Normalized input cannot change accepted host/account/session authority or configure a server; core admission decides from bounded authority facts.
