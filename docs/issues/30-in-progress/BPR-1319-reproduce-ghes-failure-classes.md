---
schema: "implement-rfc.card.v1"
id: "BPR-1319"
title: "Establish separate GHES tool and actor-provenance reproducers"
status: "In Progress"
rfcs: ["RFC-BPR-0065"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# Establish separate GHES tool and actor-provenance reproducers

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Bind actual build/platform/configured tools and bounded version/exit facts; distinguish failed probe, unsupported tool and missing source; retain logs outside commits.
- C2: Separate composed synthetic reproducers establish Git qualification and actor-login provenance including restart; do not guess legitimate identity treatment from anonymized logs.
