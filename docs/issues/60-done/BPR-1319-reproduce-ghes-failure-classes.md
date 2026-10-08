---
schema: "implement-rfc.card.v1"
id: "BPR-1319"
title: "Establish separate GHES tool and actor-provenance reproducers"
status: "Done"
rfcs: ["RFC-BPR-0065"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"e61614205ca061677846431701febd136aeed385","evidence":{"kind":"observation","detail":"Bound observed/synthetic/local tools, version and failed-probe facts; local native exact acquisition safety/object proof passes. Owner reports2.54 Apple157/gh2.101; selected affected paths/build and actual qualification are explicitly unavailable, not inferred. Original logs remain ignored."}},{"criterion":"C2","outcome":"pass","revision":"e61614205ca061677846431701febd136aeed385","evidence":{"kind":"command","detail":"Separate public synthetic GHES protocol acquisition and provider read reproducers across recreated transports/reads: Git2.54 banner qualification vs failed probe, retry/recreated authority; actor viewer/review provenance, case-only and other true conflicts still reject, corrected synthetic payload retries. Five focused tests pass; both export privacy modes preserve only closed origins/classification. No real actor representation normalization inferred."}}]
---

# Establish separate GHES tool and actor-provenance reproducers

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Bind actual build/platform/configured tools and bounded version/exit facts; distinguish failed probe, unsupported tool and missing source; retain logs outside commits.
- C2: Separate composed synthetic reproducers establish Git qualification and actor-login provenance including restart; do not guess legitimate identity treatment from anonymized logs.

## Reproduction result

Public synthetic protocol/provider courses distinguish Git qualification from
failed probing and attributable actor conflicts across recreated transports and
reads. All five focused checks pass. Exact local native Git/gh facts and direct
acquisition safety/object checks are recorded separately from the owner-reported
versions and unavailable configured paths/build correspondence. The original
exports expose neither actor values nor their collection provenance, so no
legitimate-identity repair is inferred. New closed-origin diagnostics preserve
rejection and both export privacy modes. See the shared ledger and ignored
`artifacts/four-drafts-r1/ghes-worker/1319-checkpoint.json`.
