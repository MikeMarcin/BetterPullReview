---
schema: "implement-rfc.card.v1"
id: "BPR-0955"
title: "Compose maintenance and diagnostic controls"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0954"]
milestone: "M5 Qt presentation adapter"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"13a7d0522137eeb4ab147ff9d563de62aa978035","evidence":{"kind":"observation","detail":"Pass: real profile Settings mounts signed-out native-host metadata preview and exact new-file export, with full accepted Target. Controlled host/storage/lifecycle course exercises all seven command presentations and exact clear/purge confirmation. Existing epoch retention remains unchanged. Candidate-bound reproducible commands and original evidence revisions are in docs/verification/RFC-BPR-0014.md and docs/rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m5.json."}},{"criterion":"AC2","outcome":"pass","revision":"13a7d0522137eeb4ab147ff9d563de62aa978035","evidence":{"kind":"observation","detail":"Pass: actual Settings hierarchy at 100/125/200%, keyboard/focus, one polite accessible status, default/Escape Cancel, exact inventory context, stale dialog dismissal, cancellation and partial cleanup. Actual Cocoa desktop/native-root export/refusal/shutdown passed using a deterministic Qt save picker. Full final archive passes unchanged architecture gate; no Qt effects. Candidate-bound reproducible commands and original evidence revisions are in docs/verification/RFC-BPR-0014.md and docs/rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m5.json."}},{"criterion":"AC3","outcome":"pass","revision":"13a7d0522137eeb4ab147ff9d563de62aa978035","evidence":{"kind":"observation","detail":"Pass: unchanged preview keeps document/selection/focus, equivalent actions never toggle availability, ordinary diagnostic publications reset no workbench model. Shared Settings owner-transfer regression and existing selected-review multi-window course pass; source no-impact recorded for epoch/diff/native/provider/release owners. Candidate-bound reproducible commands and original evidence revisions are in docs/verification/RFC-BPR-0014.md and docs/rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m5.json."}}]
---

# Compose maintenance and diagnostic controls

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Expose the required user-initiated diagnostic preview/export and explicit retention/purge controls through typed intents and core command projections, reusing existing controls where justified.
- AC2: Verify keyboard, focus, accessibility, cancellation and safe destructive-confirmation defaults on the real rendered application; Qt performs no filesystem, vault, provider or process effect.
- AC3: Apply bounded changed projections without routine whole-model reset; record exact source/public-path evidence for any presentation no-impact disposition.

## Verification and execution record

Implemented under mandate r3 by sole leaf `/root/rfc0014_m5`, from clean main
`d2539a90b387eedd119748d019a1490633227158`, guarded claim `46a32a10`.
Source candidate `a34fc07ffe49e21d03edce174bd28a0d5fb37a8a`, corrected
composition candidate `13a7d0522137eeb4ab147ff9d563de62aa978035`.

- AC1: Pass. Real profile Settings mounts signed-out native-host preview and
  exact local export. Seven typed command projections/intents drive controlled
  clear/purge presentation. Epoch retention keeps its existing owner. Production
  destructive lifecycle composition remains M6; its capabilities are unavailable.
- AC2: Pass. Rendered 100/125/200% courses cover keyboard/focus, one polite
  announcement, exact default/Escape confirmation, cancel/barrier/stale identity
  and partial-cleanup terminals through the real host. Actual Cocoa/native-root
  export, existing-file refusal, shared Settings and shutdown pass with a
  deterministic save picker. Complete candidate architecture passes; Qt receives
  only effect-free projections. No human/native-picker/package verdict inferred.
- AC3: Pass. Equivalent projections retain preview selection/focus and enabled
  commands; diagnostic status resets no workbench model. Existing Settings and
  selected-review multi-window regression pass. Epoch, diff/alignment, provider,
  native and release owners are unchanged.

The [M5 ledger](../../verification/RFC-BPR-0014.md#milestone-5-qt-presentation-adapter)
records exact commands/candidates, failures/corrections, rendered artifacts,
static-analysis limits and station discovery. All three card criteria and both
milestone exits are accepted for this authored presentation boundary; final
RFC/package/native/security criteria remain open.

## M2 allocation for the successor

The [allocation](../../architecture/release-readiness.md) identifies the current
Storage & privacy placeholder in `src/adapters/qt/src/settings_window.cpp`.
Mount G5/G6 controls there and preserve signed-out diagnostic access. Qt owns
preview, native save gesture, confirmation, focus/keyboard/accessibility and
bounded projection updates; typed intents carry the exact accepted snapshot/
selection identity. Cancel is the destructive-confirmation default/Escape.
No log reading, vault lookup, directory traversal or deletion belongs in a
widget. Reuse existing epoch maintenance controls and preserve all review/diff
models during ordinary status updates. New controls still need M5 evidence.

## M4 host handoff

Candidate `19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8` supplies the public
`release_maintenance_application_host`, its seven core command projections and
one immutable preview, plus native storage/local-output and exact credential
cleanup capabilities. Adopt the
[M4 contract](../../architecture/release-maintenance.md#m4-host-and-native-effects).
Qt captures semantic intent and the full accepted `Target`; its save gesture
passes a destination choice to composition for native capability admission,
never performs file effects itself. Ordinary status updates preserve review
models and do not rescan retained history.

Destructive preparation quiesces the affected profile. Composition must wire
actual window/producer invalidation, existing Git/workspace/MCP recovery,
store/logger closure and retained lease/vault-worker lifetime. Cancelling that
preparation does not silently restart producers; the user-facing continuation
must be explicit. Successful purge ends this runtime. Partial irreversible
failure requires actual recovery before fresh admission. Do not promise that
the root, shared registry/updater/sibling profiles, adjacent empty workspace or
user-exported files are deleted. Retention controls reuse their existing owner.

The host/native tests are not rendered UI or a human verdict. Complete AC2 on
the real candidate, use the configured attended-review companion when a human
decision is required, and keep package/native platform acceptance with its
assigned owners. The broader RFC-0027 controls remain deferred.
