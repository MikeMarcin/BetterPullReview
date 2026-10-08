---
schema: "implement-rfc.card.v1"
id: "BPR-1307"
title: "Publish exact durable repository addition results"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1306"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1: A signed-in session with no open PR saves through the existing queue store; saving, duplicate, local refusal and retry have exact typed results.","outcome":"pass","revision":"8367993da7c6899d88f31ecc4ae0473c5e4496f2+sha256:0e9c61bc6656ee76a7e5a2c9c83392cea7e329ba7238eeb0cf1a5a0fb0625951","evidence":{"kind":"command","detail":"Focused application_review_queue_enrollment passed after final test addition (24.44s); sqlite_queue_enrollment_storage passed (0.83s); application_personal_discovery_public_path passed (7.36s). Tests exercise typed saving/duplicate/refusal/retry, failed initial load, thrown adapter, exact auth/window replacement, reentrant retirement, shutdown drain, sibling delivery and real SQLite reopen with no PR/network. Format, focused corrected tidy and documentation pass.","manifest":"artifacts/four-drafts-r1/BPR-1307-candidate.json"}},{"criterion":"C2: Successful records survive SQLite reopen and reach sibling profile windows; pending and late completions cannot confirm a save for a replacement authority.","outcome":"pass","revision":"8367993da7c6899d88f31ecc4ae0473c5e4496f2+sha256:0e9c61bc6656ee76a7e5a2c9c83392cea7e329ba7238eeb0cf1a5a0fb0625951","evidence":{"kind":"command","detail":"Focused application_review_queue_enrollment passed after final test addition (24.44s); sqlite_queue_enrollment_storage passed (0.83s); application_personal_discovery_public_path passed (7.36s). Tests exercise typed saving/duplicate/refusal/retry, failed initial load, thrown adapter, exact auth/window replacement, reentrant retirement, shutdown drain, sibling delivery and real SQLite reopen with no PR/network. Format, focused corrected tidy and documentation pass.","manifest":"artifacts/four-drafts-r1/BPR-1307-candidate.json"}}]
---

# Publish exact durable repository addition results

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: A signed-in session with no open PR saves through the existing queue store; saving, duplicate, local refusal and retry have exact typed results.
- C2: Successful records survive SQLite reopen and reach sibling profile windows; pending and late completions cannot confirm a save for a replacement authority.
