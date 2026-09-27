---
schema: "implement-rfc.card.v1"
id: "BPR-1220"
title: "Verify and close RFC-BPR-0020"
status: "In Progress"
rfcs: ["RFC-BPR-0020"]
closeout_for: ["RFC-BPR-0020"]
depends_on: ["BPR-0295","BPR-0769","BPR-1209"]
metadata: {"mandate_revision":"r1","milestone_plan":{"id":"rfc-bpr-0020-closeout","path":"docs/rfcs/plans/rfc-bpr-0020-closeout/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0020

RFC base: `a028dcee40729457066129c73edd34f1d1820bdf`.

This immutable closeout-entry base records a documentation-only reconciliation.
Runtime implementation and prior proving revisions are retained separately in the
[ledger](../../verification/RFC-BPR-0020.md). RFC-0030/0031 supersede only their
named original restrictions; BPR-1209 carries the later corner amendment.
No old literal restriction is marked as an observed current behavior.

## Acceptance

All eleven original criteria below, the verification matrix, required attended and
independent evidence, and canonical predecessor gates must be reconciled before Done.

## Closeout checklist

| Criterion | Check, result and proving revision |
|---|---|
| The core defines and directly tests the closed `system/full/reduced` motion preference and resolution rules without importing Qt, platform, clock, filesystem, thread, or mascot frame types. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| The existing appearance host/store accepts, persists, migrates, restores, resets, rejects stale completions, and labels fallback for motion mode through its typed public path; old valid data defaults to `system` and future or malformed data fails closed. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| The selected Qt/platform adapter reports a bounded reduced-motion observation on each supported platform, a target-native before/change/after course proves the real observer and restores the original setting, change delivery is serialized, and unsupported or failed observation makes `system` static rather than silently animated. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| The Qt catalog validates and decodes the exact RFC-BPR-0019 digest-bound resources once, stays within the declared limits, and fails to a text-only landing when any resource or descriptor is wrong. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| Deterministic sequencer tests prove `greet -> calm_idle`, a closed calm looping, elapsed-time frame selection, delayed-tick coalescing, one greeting per landing generation, and static reduced/suspended behavior. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| Widget lifecycle tests prove no timer wake or repaint after hide, deactivation, review replacement, suspension, close, or destruction, and no callback accesses released presentation state. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| Qt rendering at 1.0x, 1.25x, and 2.0x across System, Light, Dark, and both high-contrast variants preserves reviewed Dif bounds, native surrounding presentation, text/control priority, focus order, accessibility, and the compact-width disappearance rule. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| The real `app_better_pull_review` public course selects `Full` and `Reduced` through the real appearance gesture and accepted host/store projection, counts exactly one mascot/catalog and at most one armed mascot timer, captures two time-separated mascot-region rasters that differ during full motion, captures a reduced-motion interval whose region remains identical, observes the greet-to-idle transition, and proves zero mascot presence/timer work after a pull request opens. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| Opening and reviewing a pull request with Dif compiled in leaves selected diff identity, navigation, drawers, focus, palette, syntax, model-reset counts, network behavior, and orderly shutdown unchanged. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| `core-only` contains no Dif resource or Qt/platform motion implementation; tests-disabled Release contains the same reviewed resource digests and no test-only clip, timer, provider, or filesystem seam. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
| Documentation and agent instructions describe ownership, preference and migration, platform support, resource selection, lifecycle, accessibility, performance bounds, public placement, failures, and exact verification without claiming semantic review behavior or package readiness. | Pending closeout audit; accepted successor evidence is being reconciled against entry source `a028dcee`. |
