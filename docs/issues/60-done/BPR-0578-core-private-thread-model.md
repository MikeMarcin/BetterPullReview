---
schema: "implement-rfc.card.v1"
id: "BPR-0578"
title: "Implement the Qt-free private thread, subject, turn, revision, actor, and lifecycle model"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0577"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Strong identities and validation","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Exact typed subjects","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Local lifecycle with derived availability","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Typed reviewer/run/legacy actor","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Immutable reviewer edit successor","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Explicit completion eligibility","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}},{"criterion":"Replay property and refusal tests","outcome":"pass","revision":"5bbb256314f32fd9082a3abcfd7df13abcd7fa55","evidence":{"kind":"observation","detail":"Public core tests and architecture checks passed; see docs/verification/RFC-BPR-0023.md M3 transition table."}}]
---

# Implement the Qt-free private thread, subject, turn, revision, actor, and lifecycle model

## Objective

Define deterministic private-conversation semantics without persistence, process, provider, or presentation dependencies.

## Required final state

- Strong IDs exist for threads, turns, and revisions, with validation and stable names where appropriate.
- Thread subjects cover PR-wide and existing exact semantic identities without presentation coordinates or null-as-PR ambiguity.
- Thread lifecycle is open/resolved-locally/archived; staleness/composition/publication are derived facts, not destructive lifecycle states.
- Every revision has exactly one typed actor: reviewer, exact agent run, or legacy unknown.
- Editing creates an immutable successor; original actor/body/digest remain unchanged.
- Complete/incomplete/failed/cancelled revision eligibility is explicit.
- Core replay/example/property tests cover creation, reply, edit, archive, stale observation, invalid identity, and no silent reanchor.

## Non-goals

- SQLite schema
- Agent protocol parsing
- Qt labels

## Completion evidence

- Focused core test command/output
- Forbidden-dependency/include check
- Transition table and rejection coverage

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
