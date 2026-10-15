---
schema: "implement-rfc.card.v1"
id: "BPR-1527"
title: "Repair final native release composition contracts"
status: "In Progress"
rfcs: ["RFC-BPR-0023", "RFC-BPR-0052"]
depends_on: []
priority: "P1"
metadata: {"source":"Final 0.2.1 macOS normal gate at d52f5bb1221e26dc00dbc9269c3ad0408b64548a","assigned_implementer":"/root/bpr1525_implementer","mandate":"authorized-release-continuation-final-gates-r3","milestone_plan":{"id":"bpr-1527-final-native-release-contracts","path":"docs/rfcs/plans/bpr-1527-final-native-release-contracts/milestone-plan.json"}}
---

# BPR-1527: Repair final native release composition contracts

The final macOS release gate exposed five failures on clean integrated source
`d52f5bb1221e26dc00dbc9269c3ad0408b64548a`: agent invocation composition,
UIforETW selected diff, threaded reply, label authoring and reply draft.
Preserve their actual output and distinguish inherited defects from regressions.
BPR-1525 and BPR-1526 remain Done with their original acceptance intact.

## Acceptance

- AC1: Retain each of the five exact-source failures and demonstrate its cause, including baseline provenance and useful counterexamples; identify any remaining uncertainty honestly.
- AC2: Threaded Reply, label authoring and reply-draft public contracts pass with exact thread, anchor, draft, publication, refresh and restart assertions intact; navigation, stale scope and unrelated drafts cannot acquire a delayed reply target.
- AC3: The agent invocation seed and reopen composition pass through the accepted immutable-source public path, preserving deliberate mutable-input refusal, unchecked-source confidentiality, exact invocation/cleanup/quiescence and zero GitHub mutation assertions.
- AC4: The UIforETW public capture passes an exact complete toolbar-order oracle including the shipped Accept file action, preserving existing appearance, geometry, navigation, pixel and height assertions; an omitted or misplaced required control fails.
- AC5: Pass focused native Mac verification of all five courses and affected portable/Windows/Ubuntu paths when their slots are released, meaningful negative controls, format 22, appropriate strict tidy, architecture and documentation checks; self-review and commit the narrow repair, and hand back exact source/command ownership with full final common-source release gates and packages still required.
- AC6: Diagnose the final d52 Windows `qt_diff_workbench_scale_200` timeout120.03s, demonstrate a bounded source-backed whole-workbench improvement and pass the normal complete course within its unchanged120s deadline under prescribed resource exclusivity, preserving all input/assertions and checking affected scales; retain the prior119.08s focused result and final-gate context honestly.
- AC7: Diagnose and repair `application_review_progress_push_contract`, all three `application_review_tour_composition` scales, `application_active_changed_scaling` and `application_review_tour_cold_import_public_path`, retaining exact durable receipt identities, tour receipt isolation, automatic-Seen policy, bounded changed-set work and meaningful counterexamples without weaker thresholds or semantic assertions.
- AC8: All three `application_source_find` scales pass their existing supported900-pixel-window, every-drawer-open floating-widget fit and query-width assertions, with exact geometry/focus/interaction and native appearance retained; counterexamples detect the demonstrated layout cause.

## Explicit scope amendment

The coordinator explicitly added the sixth criterion after the final Windows gate timed out at
test359 (346/705). The Qt courses were serial; the earlier architecture591.63s
and age180.26s courses had finished, with no competing root/WSL workload.
The 125% course was still running when the amendment arrived: do not preempt it.
All original five criteria and milestone exits remain. This is a new acceptance
unit under the existing r3 mandate, not a reopened BPR-1525 or relaxed120s gate.

The complete final Mac result was719pass/14fail/1skip, exit8. On releasing the
Mac slot, the coordinator explicitly added nine courses: review-progress push,
three tour composition scales, active-changed scaling, tour cold import and
three source-find scales. The six prior criteria and exits remain unchanged.
All fourteen exact diagnostics were read before further edits. The failed
producer remains untouched; only the owned-worktree Mac build slot is released.
Windows/WSL remain coordinator-owned while their final suite runs. The skipped
Debug cache-bytes course still requires the later optimized Release supplement.

## Ownership and limits

The coordinator allocated BPR-1527 in `26fa0b98fde716b7aefffb92da0bae7676ab3c8e`
and assigned the existing sole implementer under mandate
`authorized-release-continuation-final-gates-r3`. The assigned worktree is
`codex/v021-windows-gate-fixes`. Root main and both active final producer
suites stay untouched. No native build, test or performance process begins
until the coordinator explicitly releases its platform slot. Later failures
need an enumerated scope amendment. No assertion or threshold relaxation,
unsupported-platform waiver, signing, authentication or publication is authorized.

The [checkpoint](../_checkpoints/BPR-1527/CURRENT.md) records the diagnosis,
runtime binding and native continuation frontier.
