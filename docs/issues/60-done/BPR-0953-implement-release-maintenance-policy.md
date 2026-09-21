---
schema: "implement-rfc.card.v1"
id: "BPR-0953"
title: "Implement release maintenance policy"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0952"]
milestone: "M3 Deterministic functional core"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7445b09efd0f831ae0838f1eee3c430cdc248ebc","evidence":{"kind":"observation","detail":"Implemented release_maintenance fixed-size deterministic preview/export/clear/purge identity, selection, bounds, confirmation and cancellation/recovery policy; exact epoch retention and appearance/theme migration-value source/tests reused. See docs/architecture/release-maintenance.md and M3 ledger. Effects remain M4."}},{"criterion":"AC2","outcome":"pass","revision":"7445b09efd0f831ae0838f1eee3c430cdc248ebc","evidence":{"kind":"command","detail":"Exact core-only build plus 8 focused core/boundary tests, new pure API course under ASan/UBSan, complete git-archive architecture scan pass. Architecture hostile fixture contract passes with unchanged relevant inputs; 256 replay courses and all context/selection refusals are asserted. Receipts build/rfc0014-run/m3-*-evidence.json."}},{"criterion":"AC3","outcome":"pass","revision":"7445b09efd0f831ae0838f1eee3c430cdc248ebc","evidence":{"kind":"observation","detail":"Contract names exact retained epoch, appearance/theme, profile and updater source/tests and host-owned store/credential no-impact regions; scoped diff confirms unchanged owners/release tools. Format/tidy, documentation, board, plan and unchanged canonical criteria pass. No native/human/independent acceptance claimed."}}]
---

# Implement release maintenance policy

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Implement or directly prove reusable deterministic retention, purge, migration and diagnostic-selection/redaction policy with compact state and explicit operation identity.
- AC2: Cover permitted/refused/destructive/stale transitions with direct core tests that require no QApplication, Qt, provider, clock, filesystem, thread or other effects.
- AC3: Record any no-impact core region with exact existing source/test evidence; do not add policy merely to fill a plan stage.

## Verification and execution record

Claimed at `d6a39867839283b12e0cdb597e605018041814fd`, from clean main
`9ceb9b0af2eff3c015203a48ff7dd12cc1c55b18`, by `/root/rfc0014_m3`, mandate r3.
The [M3 binding](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m3.json)
retains all three criteria and both milestone exits. RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`.

The [core contract](../../architecture/release-maintenance.md) records the
new bounded, allocation-free transition and the exact unchanged semantic
owners. Initial public core courses pass: epoch retention, appearance values,
profile ownership and updater policy. The new preview/export, clear and purge
courses pass; no Qt/application host effects are claimed by those courses.
Accepted source candidate: `7445b09efd0f831ae0838f1eee3c430cdc248ebc`.

- AC1: The public `release_maintenance` transition implements bounded metadata
  selection, exact preparation/confirmation, cancellation, generation and
  recovery policy; the existing retention and migration-value owners are reused.
  State is trivially copyable and at most 512 bytes. Native effects are M4 work.
- AC2: Eight focused core/boundary checks pass at the exact candidate, including
  `core_release_maintenance`'s permitted/refused/destructive/stale/duplicate,
  identity-exhaustion and 256-course replay assertions. The same new course
  passes standalone AddressSanitizer/UndefinedBehaviorSanitizer with only the
  new core/test translation units. Core linkage contains only system C++/C.
- AC3: The core contract names exact epoch, appearance/theme, profile and
  updater source/tests and the unchanged schema/credential effect owners.
  Scoped source comparison confirms reuse and no host/Qt/release-tool changes.

The architecture scanner passes the complete exact-candidate Git archive;
`architecture_boundary_contract` passes (90.28 seconds) with unchanged relevant
fixture/checker inputs. The archive avoids the unrelated ignored nested
worktree that caused M2's checkout scan failure; that historical failure remains
recorded. Touched-file format and clang-tidy pass, as do documentation, scoped
board, plan discovery and whitespace gates. Local exact-candidate receipts are
`build/rfc0014-run/m3-*-evidence.json`; the
[ledger](../../verification/RFC-BPR-0014.md#milestone-3-deterministic-functional-core)
records commands, useful failures and limits. The code-simplifier/self-review
pass kept shared command admission, fixed-size values and early refusal helpers.
No independent, native or human acceptance is required for M3 or claimed here.

## M2 allocation for the successor

The [source allocation](../../architecture/release-readiness.md) is the r3
starting point. Reuse the epoch maintenance transition/identity and each
store's migration authority; qualify them directly instead of creating a
generic migration engine. New core work is compact diagnostic snapshot
selection/preview-export identity, clearing, and exact profile-purge eligibility
(G5/G6). Define bounds, allowed categories, confirmation/cancellation and stale/
duplicate refusals with no native path as authority. M4 owns all observations
and effects. RFC-0027 is a compatibility reference, not authorization to activate
its deferred BPR-0603 or optional broad logging features. No new policy is
implemented or accepted by M2.
