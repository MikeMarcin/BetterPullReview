---
schema: "implement-rfc.card.v1"
id: "BPR-0763"
title: "Route existing accepted outcomes to truthful Dif cues"
status: "Done"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0752", "BPR-0760"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"context-integration","acceptance":["B7","B8"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"accepted contextual facts map to their specified clips","outcome":"pass","revision":"3a00929154dc913249c48b1aa866eb8b9651947a","evidence":{"kind":"command","detail":"qt_dif_runtime and the existing invocation/chat/finding/draft Qt contracts passed; the normal application target compiled with post-presentation typed fan-outs for start, delivery, terminal, workshop, continuation and accepted draft."}},{"criterion":"terminal outcome coalesces blocked over attention over ready without process-only success","outcome":"pass","revision":"3a00929154dc913249c48b1aa866eb8b9651947a","evidence":{"kind":"observation","detail":"The bounded Qt-local router accumulates only accepted presented facts and selects blocked, attention, comment_ready, or none when the matching terminal is presented; a success with no content produces no cue."}},{"criterion":"current-operation latches reject replay and stale results without unbounded state","outcome":"pass","revision":"3a00929154dc913249c48b1aa866eb8b9651947a","evidence":{"kind":"command","detail":"qt_dif_runtime exercised duplicate start/terminal, superseded invocation, wrong PR, hidden completion, exact workshop/draft revision, continuation dedup and cooldown; the router retains only current/terminal/draft/continuation identities."}}]
---

# BPR-0763: Route existing accepted outcomes to truthful Dif cues

## Outcome

Implement the Qt-local cue router and narrow app projection fan-outs, preserving current strong identities and validated findings-before-finish ordering. Do not add provider work or parse output text.

## Acceptance

- Start, accepted reply/proposal, triage batch, accepted local draft result and real failure/continuation each map exactly as specified.
- Terminal outcome coalesces blocked over attention over ready, with no success from process exit alone.
- Current-operation latches reject replay/stale result; no unbounded queue/history set.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
