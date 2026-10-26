---
schema: "implement-rfc.card.v1"
id: "BPR-1770"
title: "Batch direct Home review membership and explain refresh work"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner authorization 2026-10-26 via coordinator; mandate home-requested-repairs-r1","reservation":"docs/issues/_reservations/BPR-1770--BPR-1770--0361a441a92d.json","baseline":"e6e69cc2c6a429209ac3462579a4cb685855fa86","related_cards":["BPR-1680","BPR-1694","BPR-1769"],"milestone_plan":{"id":"home-requested-repairs-20261026","path":"docs/rfcs/plans/home-requested-repairs-20261026/milestone-plan.json"}}
---

# Batch direct Home review membership and explain refresh work

The owner authorized all three requested-review performance repairs and asked
that the export checkbox say “Include verbose logs from the last 3 minutes”.
The supplied private diagnostic export showed 37 requested-discovery requests
and about 27 seconds, with no timeout, failure or lane wait; its composition
cannot be reconstructed as a particular count of team-only candidates.

## Acceptance

- AC1: Search uses the direct-user qualifier supported by GitHub.com and the
  selected GHES contract; whitelist, query bounds and repository batching agree.
  Search hits never establish membership or replace exact account authority.
- AC2: Unknown requested candidates use bounded GraphQL membership batches,
  combining identity anchor and first roster page. Absence requires exhaustive
  pagination, with identity/revision, alias, cancellation and size/work bounds.
  Supported fallback stays safe; only direct matches receive optional summaries.
  Repeated refreshes revalidate negative membership. Existing full Home anchor
  recovery uses the narrow membership projection.
- AC3: Standard and anonymized exports carry closed counts for candidates,
  retained reuse, direct matches, rejections and roster recovery, plus the exact
  completeness reason through provider, host and core. Old log lines still parse;
  bounded formatter/parser and export allowlists reject extra content.
- AC4: Checkbox plainly names supplemental in-memory verbose logs from the last
  three minutes, with unchanged default, privacy and retention behavior.
- AC5: Meaningful regression and composed transport/request-count tests pass,
  alongside focused security, format/tidy, core/boundary and documentation checks.

## Implementation plan

One milestone: establish a failing request-count regression; add a closed
membership query and strict paginated parser; use it for requested candidates
and recovery; carry safe discovery counts; update wording; verify and self-review.
No private-server retest or native performance measurement is claimed.
