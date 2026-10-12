---
schema: "implement-rfc.card.v1"
id: "BPR-1394"
title: "Show pending reviewers and merge requirements in Overview"
status: "In Progress"
rfcs: ["RFC-BPR-0069"]
depends_on: ["BPR-1393"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Show pending reviewers and merge requirements in Overview

## Scope and acceptance

Own the Overview projection and native roster/requirements presentation. Integrate with check details so simultaneous review and check blockers remain visible. Preserve submitted dispositions alongside re-requests and readable organization-qualified team names.

Authoritative acceptance: [RFC-BPR-0069](../../rfcs/RFC-BPR-0069-reviewers-and-merge-requirements.md#acceptance), AC01, AC02, AC03, AC04, AC05, AC06. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Use the composed three-team scenario and ordinary controls. Verify approval-count and auto-merge wording against supplied evidence and inspect long-name/narrow-width renders. All actions in this new surface are admitted reads or explicit external handoffs.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
