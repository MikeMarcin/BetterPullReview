---
schema: "implement-rfc.card.v1"
id: "BPR-0943"
title: "Resolve modified-file endpoints without silent missing-side diffs"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r21-drawer-and-exact-material"}
---

# Resolve modified-file endpoints without silent missing-side diffs

Owner-observed signed r22 defects extend the current selected-diff-exact-epoch-pair
milestone. Prior BPR-0935..0939 acceptance remains historical; these are new
follow-ups, not reopened Done cards. Preserve BPR-0934 Overview folder collapse
as separate work and do not request another owner loading test.

## Acceptance

- AC1: Attribute the owner-observed modified file with empty earlier pane using exact selected paths, statuses, commit/tree modes/OIDs, sparse residency, cache and diff inputs; retain only privacy-safe facts.
- AC2: Missing required endpoint material produces explicit unavailable/error presentation rather than a successful empty side; legitimate absence and alternate selected comparisons remain truthful and exact.
- AC3: A failing-before production-composed regression reproduces the observed class, with installed Git/cache checks, stale/cancellation safety and signed tests-disabled safe native rendered evidence.
- AC4: No live GitHub mutation, account change, private source/pixel/OID or credential retention occurs; original private-target source/anchor obligation remains explicit.

## Evidence

Pending direct diagnosis, failing-before regressions and repaired public-path proof.
