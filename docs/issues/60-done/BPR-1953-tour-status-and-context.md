---
schema: "implement-rfc.card.v1"
id: "BPR-1953"
title: "Give tour sessions authoritative state and source-status reasons"
status: "Done"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1952"]
acceptance: [{"criterion":"M3 guide/snapshot/freshness and source distinctions","outcome":"pass","revision":"a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8","evidence":{"kind":"observation","detail":"Functional milestone evidence and explicitly retained later installed/native gaps are recorded in docs/verification/RFC-BPR-0096.md.","refs":["docs/verification/RFC-BPR-0096.md","m3-public7.log","m3-integration7.log","m3-tour-integration8.log","m3-core-check7.log","m3-render8.log","m3-tidy7.log","m3-format-final7.log","m3-doc-product7.log","m3-doc-tests6.log"]}},{"criterion":"M3 submitted-ID update/confirmation/publication boundaries","outcome":"pass","revision":"a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8","evidence":{"kind":"observation","detail":"Functional milestone evidence and explicitly retained later installed/native gaps are recorded in docs/verification/RFC-BPR-0096.md.","refs":["docs/verification/RFC-BPR-0096.md","m3-public7.log","m3-integration7.log","m3-tour-integration8.log","m3-core-check7.log","m3-render8.log","m3-tidy7.log","m3-format-final7.log","m3-doc-product7.log","m3-doc-tests6.log"]}},{"criterion":"M3 direct public composition, rendering and focused verification","outcome":"pass","revision":"a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8","evidence":{"kind":"observation","detail":"Functional milestone evidence and explicitly retained later installed/native gaps are recorded in docs/verification/RFC-BPR-0096.md.","refs":["docs/verification/RFC-BPR-0096.md","m3-public7.log","m3-integration7.log","m3-tour-integration8.log","m3-core-check7.log","m3-render8.log","m3-tidy7.log","m3-format-final7.log","m3-doc-product7.log","m3-doc-tests6.log"]}}]
---

# Give tour sessions authoritative state and source-status reasons

## Scope and acceptance

Implement the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
versioned BPR tour guide, structured snapshot and current-state updates in
native and replay contexts. Refresh on all relevant BPR fact changes and
unestablished bootstrap after compaction/reconnect, only with explicit sends.
Derive distinct source confirmation, preparation, mapping, reaffirmation,
Ready/publication eligibility and corrective actions from core facts shared
with Qt, including whole-tour blockers and captured stable stop identities.
Correct only the bounded misleading warning behavior specified in the RFC;
preserve anchor identity on text/order edits and publication/acceptance
boundaries. This is the `tour-context` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover AC08 and AC09 and the tour-state-update portion of AC07. A composed
warning/update course contributes to AC10. Retain the distinction between
source diagnosis and the owner's not-yet-reproduced exact sequence.


## Verification and execution record

Mandate `native-agent-sessions-r1`; implementer `/root/native_sessions_m3`.
Claim `75bd8d369b2750502c28ada149bb846d6a94a976`; product candidate
`a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8`. The [ledger](../../verification/RFC-BPR-0096.md)
records versioned complete bootstrap, shared core source/preparation/publication
reasons, explicit unknown evidence and bounded proposal outcome provenance.
Captured stable IDs protect reorder/late results. Actual visible-composer update
and author acceptance preserve confirmation across both fixture protocols and
application reopen; follow-up snapshots contain accepted decisions/current prose.
Confirmed author-selected and ambiguous mappings have distinguishing Qt assertions
and inspected offscreen rasters. Source text/order semantics remain covered by
core and the normal tour composition course.

Four public seed/reopen courses pass; nine integration checks plus the separately
rerun tour composition course pass; four focused and four core-only checks pass.
clang-format22, scoped tidy22 on nine implementation units, structure gate and67
script tests pass. An initial missing-helper integration refusal and other actual
failures/corrections remain in the ledger and external logs. No validator was
relaxed. Frontend-design was loaded; implementation used offscreen rendering only.

This completes the milestone's functional boundary. Full AC08 installed-agent
explanation and AC07/AC10 native qualification remain on BPR-1955, alongside the
signed-out installed Claude, native platform/resource and Release cost gaps.
BPR-1954 retains replay/legacy/quota/history/save-failure work. The owner's exact
original all-stops warning sequence is not claimed reproduced. No human decision
is newly parked and no attended candidate exists. Handoff:
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m3-handoff.md`.
