---
schema: "implement-rfc.card.v1"
id: "BPR-0991"
title: "Import the authored pack and reconcile the live baseline"
status: "Done"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: []
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-01","milestone":"intake","acceptance":[],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
acceptance: [{"criterion":"intake-lineage","outcome":"pass","revision":"129432b3941d21edd4bee84b7edec77a32562b64","evidence":{"kind":"observation","detail":"Original 684858ef import and allocator receipt retained. All five RFC identities, all 26 aliases, all eight authored milestones and source digest checked by focused lineage assertions; pack validation passed 82 checksums. See docs/verification/RFC-BPR-0039-0043.md."}},{"criterion":"intake-baseline","outcome":"pass","revision":"129432b3941d21edd4bee84b7edec77a32562b64","evidence":{"kind":"observation","detail":"Compared all 33 top-level SOURCE-BASELINE paths: 17 exact hashes, 16 changed, none missing. Inspected current publisher, native Markdown/editor, settings, storage v3, RFC-0052 composition and test isolation seams; documented gaps and other-checkout ownership in docs/verification/RFC-BPR-0039-0043.md."}},{"criterion":"intake-plan-board","outcome":"pass","revision":"d871ac95baac8fa23b152df8082351c1dc90d4c0","evidence":{"kind":"command","detail":"Installed milestone plan discover/check and milestone checkpoint bind passed. Scoped board validation passed. Documentation gate DOCUMENTATION_OK; documentation regression suite 64/64; git diff --check passed. Semantic evidence judgment is limited to intake, not product acceptance."}}]
---

# BPR-0991: Import the authored pack and reconcile the live baseline

Pack alias: **PACK-TOUR-01** (live `BPR-0991`, allocated at import).

## Outcome and work

Record live HEAD, current RFC/issue allocations, current test isolation and affected in-flight ownership. Reserve/remap proposed RFC IDs if occupied, allocate issue IDs with the existing allocator, retain receipts, import the authored milestone plan with the installed skill, and preserve this archive as source. Check the top-level source against SOURCE-BASELINE rather than embedded worktree copies. Record exact existing publisher, Markdown, editor, settings and storage seams once.

## Existing ownership seams

`AGENTS.md; docs/rfcs/README.md; scripts/allocate_issue_id.py; .mask/skill-config/implement-rfc/`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Native plan/board validation after alias remap; explicit mapping for all five RFCs and 26 card aliases; one source-drift note. Do not run company PR mutations or change existing RFC status.

## Integration boundary

Milestone: **intake**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
