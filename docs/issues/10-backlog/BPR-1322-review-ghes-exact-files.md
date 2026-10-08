---
schema: "implement-rfc.card.v1"
id: "BPR-1322"
title: "Verify the affected enterprise PR read-only on an exact candidate"
status: "Backlog"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1320", "BPR-1321"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# Verify the affected enterprise PR read-only on an exact candidate

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: On an exact self-contained frozen repaired candidate, owner opens the originally affected PR read-only without supported-snapshot rejection and views intended exact contents/diffs.
- C2: Fresh short session asks one Accept/Reject rendered-outcome criterion; record actual build/tool/transport qualification, retain private target/logs outside commits and no live mutation.

## Owner strategy change: affected evidence deferred to pre-release (2026-10-07)

The owner cannot answer the native diagnosis review. Session
`b366df1f04624eb90e90d47c9262e52d` is **withdrawn as unanswerable**: an
objective observation records the withdrawal on each of its three candidates
(cursor 12). No verdict exists; nothing was accepted or rejected. Its server
was stopped and candidate v001 stays protected.

The strategy is now to diagnose from the two existing anonymized diagnostics
logs (private, ignored and never committed), make the most defensible fix and
take affected-installation confirmation from the next pre-release.

**Re-scope.** The owner's read-only check of the originally affected PR on
an exact candidate is deferred to the next pre-release. C1 and C2 remain the
acceptance and an explicit open gap. No fixture, local or capability evidence
substitutes for them, and the incident is not claimed fixed.
