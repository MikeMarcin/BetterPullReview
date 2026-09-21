---
schema: "implement-rfc.card.v1"
id: "BPR-0955"
title: "Compose maintenance and diagnostic controls"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0954"]
milestone: "M5 Qt presentation adapter"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
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

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

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
