---
schema: "implement-rfc.card.v1"
id: "BPR-1313"
title: "Persist each window summary fraction through its existing host"
status: "Ready"
rfcs: ["RFC-BPR-0063"]
depends_on: ["BPR-1312"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# Persist each window summary fraction through its existing host

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Explicit preferred fraction survives refresh/navigation/full reader/restart per window; clamp does not overwrite intent, reset clears it and empty body preserves it.
- C2: Authority replacement clears old text/reading before new content; stale results cannot repopulate it; only size is persisted.
