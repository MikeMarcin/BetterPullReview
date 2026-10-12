---
schema: "implement-rfc.card.v1"
id: "BPR-1397"
title: "Explain pending reviews and concurrent blockers on Home"
status: "Done"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1396"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"39ecdbe1b544e18c12275c95db46734ee3937ecc","evidence":{"kind":"observation","detail":"Native and CLI discovery → accepted Overview → Home courses preserve three named code-owner requests; host unread/read transitions preserve fields. Final public6/6, clearance5/5.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log"]}},{"criterion":"AC02","outcome":"pass","revision":"39ecdbe1b544e18c12275c95db46734ee3937ecc","evidence":{"kind":"observation","detail":"Exact provider actor ownership is projection-only for Follow and observer enrollment; aggregate review with no names waits; Unfollow removes a Follow-only row; numeric-account isolation passes.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log"]}},{"criterion":"AC03","outcome":"pass","revision":"39ecdbe1b544e18c12275c95db46734ee3937ecc","evidence":{"kind":"observation","detail":"Three-team Branch Freshness public example remains waiting; host unread/clearance and complete removal/approval preserve independent blockers; native Quiet/accessibility checks pass.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log"]}},{"criterion":"AC04","outcome":"pass","revision":"39ecdbe1b544e18c12275c95db46734ee3937ecc","evidence":{"kind":"observation","detail":"Host/provider/storage dispatch order, omitted versus empty, head/account/session, unavailable refresh, optional write failure, restart/migration and bounded minimal reads pass; final codec recheck2/2.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log"]}},{"criterion":"AC05","outcome":"pass","revision":"39ecdbe1b544e18c12275c95db46734ee3937ecc","evidence":{"kind":"observation","detail":"Native role/Quiet/collapsed counts/accessibility and composed exact selection/open/sibling/filter/Recent courses pass; Home slow-open passes and final Overview scales1/1.25/2 pass. Notification policy unchanged.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log"]}}]
---

# Explain pending reviews and concurrent blockers on Home

## Scope and acceptance

Own the native Home row/section explanations and filtered summaries from the corrected core projection. Show pending users or code-owner teams and any concurrent check block, preserving grouping, selection and exact opening destination.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Run the composed Home → Overview → Home scenario, including unread activity and its clearance, a final request disappearing while a requirement remains, and long team names. Check Quiet-items behavior and unchanged notification/enrollment boundaries.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
