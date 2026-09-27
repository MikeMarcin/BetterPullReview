---
schema: "implement-rfc.card.v1"
id: "BPR-1005"
title: "Track changed source and tour revisions without false reanchoring"
status: "Done"
rfcs: ["RFC-BPR-0042", "RFC-BPR-0040"]
depends_on: ["BPR-1004", "BPR-0998"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-15","milestone":"manual-maintenance","acceptance":["AC42-06","AC42-08","AC40-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC42-06","outcome":"pass","revision":"22fd215079fa6ccf13765db6f19b445c49ee4fad","evidence":{"kind":"observation","detail":"Core/diff/store/app selector 6/6 passes; renamed/moved/changed/missing/ambiguous/unavailable and never-retained source; head reaffirmation preserves remote source; cold unknown history and changed-stop navigation issue no review effects. See docs/verification/RFC-BPR-0039-0043.md BPR-1005."}},{"criterion":"AC42-08","outcome":"pass","revision":"22fd215079fa6ccf13765db6f19b445c49ee4fad","evidence":{"kind":"observation","detail":"Core/diff/store/app selector 6/6 passes; renamed/moved/changed/missing/ambiguous/unavailable and never-retained source; head reaffirmation preserves remote source; cold unknown history and changed-stop navigation issue no review effects. See docs/verification/RFC-BPR-0039-0043.md BPR-1005."}},{"criterion":"AC40-08","outcome":"pass","revision":"22fd215079fa6ccf13765db6f19b445c49ee4fad","evidence":{"kind":"observation","detail":"Core/diff/store/app selector 6/6 passes; renamed/moved/changed/missing/ambiguous/unavailable and never-retained source; head reaffirmation preserves remote source; cold unknown history and changed-stop navigation issue no review effects. See docs/verification/RFC-BPR-0039-0043.md BPR-1005."}}]
---

# BPR-1005: Track changed source and tour revisions without false reanchoring

Pack alias: **PACK-TOUR-15** (live `BPR-1005`, allocated at import).

## Outcome and work

Keep original sources and proposed later mappings distinct. Mark changed/missing/ambiguous code/context for author attention and do not reinterpret an old thread as a new native anchor. Add locally evidenced changed-tour navigation where available, with unknown history when no baseline exists. Never turn tour navigation into viewed/accepted/resolved review state.

## Existing ownership seams

`Existing epoch/update identity and source correspondence; normal conversation navigation`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Renamed/moved/changed snippets, identical snippet with changed surrounding contract, force push, never-retained epoch, code-unavailable comments and clean-profile unknown history; zero automatic review-status effects.

Requirement ownership: `AC42-06`, `AC42-08`, `AC40-08`.

## Integration boundary

Milestone: **manual-maintenance**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
