---
schema: "implement-rfc.card.v1"
id: "BPR-0792"
title: "Distinguish large pull request total from returned commits"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0643"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Distinguish large pull request total from returned commits

The BPR-0643 read-only diagnostic reached `GetPullRequest` for the exact
owner-retained stress target with HTTP 200, then refused its reported 9,311
total commits against the 250-record returned-commit budget. BPR-0643 is Done
for its then-current known-limit outcome and remains terminal. This is a new
RFC-BPR-0013 limit repair under the owner's renewed read-only mandate. The
product must not invent unavailable history or raise its 250-record budget.

## Acceptance

- The provider accepts a metadata total above 250 only when the bounded pull
  request commit endpoint returns exactly its documented 250-record ceiling,
  ends at the accepted head, and all existing page, identity, text, and total
  snapshot checks pass. A smaller or larger returned collection refuses
  atomically; totals at or below 250 remain exact.
- The snapshot and visible count distinguish the reported total from the
  returned records; no missing commit is fabricated or presented as retained.
- A focused synthetic positive and negative course passes. Attempt the exact
  target read-only through the selected `gh` account without exposing its
  credential, and exercise a tests-disabled public product path where feasible.
  Record the first remaining refusal with fixed, metadata-only evidence if
  the target still cannot render its file list and selected diff.
- No live pull request write, credential transfer, private content in the
  checkout or logs, global `gh` account change, or device-session change occurs.
