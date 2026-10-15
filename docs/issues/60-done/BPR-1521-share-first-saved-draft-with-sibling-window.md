---
schema: "implement-rfc.card.v1"
id: "BPR-1521"
title: "Share a first-saved inline draft with an untouched sibling window"
status: "Done"
rfcs: []
depends_on: ["BPR-1520"]
priority: "P1"
metadata: {"source":"Coordinator combined testing-note integration regression","assigned_implementer":"/root/bpr1520","related_cards":["BPR-1520"],"baseline":"3c24b9ec834fcafc87af05680f9f2799708b4267","milestone_plan":{"id":"bpr-1521-sibling-first-save","path":"docs/rfcs/plans/bpr-1521-sibling-first-save/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f364022cc97ebc472e5b40be1a66eab757d79f6d","evidence":{"kind":"observation","detail":"Exact integrated baseline3c24b9ec8 reproduced accepted draft edit reaches sibling editor in31.78s; isolated own synthetic profile and unchanged original body assertions."}},{"criterion":"AC2","outcome":"pass","revision":"f364022cc97ebc472e5b40be1a66eab757d79f6d","evidence":{"kind":"observation","detail":"Expanded composed multiwindow test passed23.20s: distinct per-consumer placeholders become the same exact durable draft, accepted body fans out and sibling edits return through shared owner. Full authority admission plus exact scope/anchor/reply and shown-selection guards retained."}},{"criterion":"AC3","outcome":"pass","revision":"f364022cc97ebc472e5b40be1a66eab757d79f6d","evidence":{"kind":"observation","detail":"Composed unrelated-anchor publication leaves untouched sibling unchanged. Held same-anchor concurrent first saves preserve separate durable IDs and exact multiline bodies, no rejected candidate; later edit leaves other authored record unchanged. Code refuses placeholder following for local candidate/pending save, different runtime scope, anchor, reply and inactive incoming state. Existing host/editor scoped refusal and identity/coalescing tests pass."}},{"criterion":"AC4","outcome":"pass","revision":"f364022cc97ebc472e5b40be1a66eab757d79f6d","evidence":{"kind":"observation","detail":"Final composed multiwindow passes23.20s. Same production source passes private-review host4.68s, Qt editor0.45s, publication recovery22.25s including application reconstruction/restart and failure/navigation, composition contract3.52s. Clang-format22, documentation gate and67 doc tests pass. Production mutation-branch pin unchanged; self-review only, no unperformed human/native/independent claims."}}]
---

# BPR-1521: Share a first-saved inline draft with an untouched sibling window

The combined application_profile_multi_window_public_path check fails at “accepted draft edit reaches sibling editor”. BPR-1520 correctly made line-selection placeholders runtime-only, but the first saved edit receives a durable draft identity while an untouched sibling editor still holds its own placeholder identity. The existing shared publication subscription therefore ignores the durable record. This is a follow-up integration regression; keep BPR-1520’s historical acceptance unchanged.

## Acceptance

- AC1: Reproduce the composed sibling-editor failure on the integrated BPR-1520 baseline.
- AC2: After a first durable edit, an untouched sibling for the same complete authority, exact anchor and reply identity adopts the durable draft and displays the accepted text.
- AC3: Never overwrite an independently authored sibling candidate, pending save or different scope/anchor/reply. Preserve both authored candidates when reconciliation cannot safely merge them.
- AC4: Verify actual composed multiwindow propagation plus focused identity/save races, restart and existing BPR-1520 durability/recovery checks. Do not weaken the existing content assertions to accommodate lost sharing.

## Coordination

Correct this before declaring the combined testing-note batch accepted. The BPR-1520 implementer owns diagnosis and the bounded correction; main integration remains coordinator-owned. No owner profile, live GitHub review, desktop or browser data is inspected or changed. Follow existing layer boundaries and frontend-design requirements for any UX edits.

## Diagnosis and correction

The isolated integrated baseline `3c24b9ec834fcafc87af05680f9f2799708b4267` reproduced the original composed failure at “accepted draft edit reaches sibling editor” in 31.78 seconds. The shared host correctly keeps each empty selection runtime-only; the application subscription previously followed only an identical draft ID.

The subscription now permits an untouched runtime-only selection to follow a committed active draft in the same runtime scope, exact source anchor and reply target. It requires the core's unused-draft predicate, a durable incoming identity, matching displayed selection, no local candidate and no save pending. Existing real-ID updates keep their ordinary conflict/acknowledgement path. A simultaneous author retains its own first-save acknowledgement and distinct saved comment. The host's full authority admission and durable-before-projection ordering are unchanged.

## Verification and source review

- `application_profile_multi_window_public_path` passes with its original bidirectional body assertions. Additional assertions prove separate runtime placeholders become the same exact durable record, unrelated anchors remain untouched, and two same-anchor authors held behind the real persistence lane keep distinct durable identities and exact multiline bodies. A subsequent edit preserves the other author's saved text; both can select the original shared comment again. New line-selection steps wait for the actual accepted draft anchor before typing.
- `application_private_review_host`, `qt_review_draft_editor` and `application_label_publication_recovery` pass on the same production source. These retain first-save atomic failure/coalescing, scoped identity refusal, rejected/indeterminate publication, refresh/navigation and application reconstruction/restart evidence. The final multiwindow rerun changes only the test's waits for asynchronous selection completion.
- `application_review_round_trip_composition_contract` passes after scoped self-review and resealing the full application/manifest pins. Only the shared subscription in `github_application.cpp` changes in the production manifest over `3c24b9ec8`; no provider, publication grant, credential route, source inventory or mutation branch changes. The production mutation-branch digest remains `e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.
- Clang-format 22 and the documentation gate pass; all 67 documentation script tests pass. The frontend-design skill was already loaded for the underlying native editor work; this followup changes no layout or copy. Automated native Qt interaction runs offscreen on macOS. Windows/Linux native execution and independent/human review are not claimed.

BPR-1520's historical acceptance and checkpoint remain unchanged. No owner profile, real review, live GitHub, desktop or browser data was read or modified. Main integration and the combined-source pin reseal remain with the coordinator.
