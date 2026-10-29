---
schema: "implement-rfc.card.v1"
id: "BPR-1814"
title: "Compile the application host as a large MSVC object"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b1606309e6c9e6655be8edeb7586dc317f0f574c","evidence":{"kind":"observation","detail":"Private MSVC /bigobj on owning application target only. Actual generated owning/consumer ClCompile controls preserve every other option and private propagation boundary; actual corrected target and full standard Debug ALL compile. Non-MSVC source/config bridge retains unchanged Mac/GCC behavior.","references":["prefreeze-target-regression-r1.json","prefreeze-windows-r2/receipt.json","mac-msvc-source-bridge-r1/receipt.json"]}},{"criterion":"AC2","outcome":"pass","revision":"b1606309e6c9e6655be8edeb7586dc317f0f574c","evidence":{"kind":"command","detail":"All corrected Windows Release/standardDebug/distribution and Ubuntu Release/standardDebug/distribution compilation proofs pass; accepted Mac proofs explicitly bridged, with actual N0 image metadata/hashes. Focused17 release-build contracts and direct composition inventory pass. Original C1128/observer/refusals/intentional capacity pauses preserved. Successor allocation and production remain separate; no N0 image relabel.","references":["prefreeze-compilation-admission-r1.json","prefreeze-ubuntu-r7/receipt.json","prefreeze-windows-r3-metadata/receipt.json","reused-mac-test-graph-identities-r1.json","reused-windows-test-graph-identities-r1.json","reused-ubuntu-test-graph-identities-r1.json"]}}]
---

# Compile the application host as a large MSVC object

The fresh Windows standard Debug compilation barrier for frozen 0.4.1 build 3
failed with MSVC C1128 in `epoch_application_host.cpp`: the COFF object exceeded
its section limit. The same source's complete Release compilation passed. The
barrier stopped before Ubuntu compilation or any long qualification suite.
Build 3's public Mac and Windows assets remain immutable and their actual
verification remains recorded; this correction requires a new candidate number.

## Acceptance

- AC1: Enable MSVC's large-object format privately on the owning application
  target, following the existing composition-target treatment. Preserve normal
  Debug symbols, optimization, runtime checks, warning and hardening flags;
  leave non-MSVC compilation and downstream public compile requirements intact.
- AC2: Retain the original C1128 result and demonstrate the corrected native
  target and complete Debug graph compile. Inspect actual generated/native
  arguments and run focused source/documentation checks. Complete the unreached
  Ubuntu Release, Debug and distribution compilation before allocating the
  successor, with an explicit bridge for unaffected Mac evidence and all prior
  signed producer images, maps, source and configuration preserved.

## Evidence and scope

The original terminal result is in external
`pc-compile-barrier-r1/receipt.json` on the approved PC, with child `17800`
exiting 1 at `2026-10-29T18:15:43.900967Z`. It receives no passing credit.
The correction changes the compiler's object format capacity, not application
behavior or qualification policy. New functional failures or hangs still require
investigation; all matching release waivers retain their exact scope.

## Verified correction and preallocation boundary

The private MSVC `/bigobj` option follows the existing composition treatment.
Actual generated owning-target options differ only by that option; the consumer
comparison retains its prior options and does not inherit the private flag. The
original C1128 result and the first comparison adapter's inherited-placeholder
refusal remain recorded. The corrected owning target, full standard Windows
Debug ALL, Windows Release ALL and tests-off distribution all compile.

Ubuntu Release ALL, standard Debug ALL and tests-off distribution now also
compile at corrected preallocation source `b1606309e6c9e6655be8edeb7586dc317f0f574c`.
The actual native receipts are retained under the external run root, including
`prefreeze-compile-r2`, `prefreeze-compile-r3`, `prefreeze-compile-r6` and
`prefreeze-compile-r7` on the approved PC. Intentional capacity pauses retain
their actual nonzero exits; accepted completed outputs were preserved and
resumed with unchanged flags under a fixed 32 GiB physical-host reserve.

The accepted Mac compile evidence remains its actual unnumbered source
`22022f13b1096601ee7ebe1536f8cc8b8156f893`, with an explicit MSVC-only source
bridge and fresh unchanged native configuration. Actual generated metadata
and primary-image hashes are recorded for all reused Release/Debug graphs.
Windows and Ubuntu test images report source `b1606309e6c9e6655be8edeb7586dc317f0f574c`
and build number zero; none is called a newly compiled numbered successor.

Focused verification retains 17 release-build contracts, the direct composition
source-inventory check and the generated native owning-target/private-consumer
controls. The next source transition may contain this card's closeout and the
allocator's counter/receipt only; those deltas require an explicit bridge.
Actual numbered production, source/security/native trust, public previews and
comprehensive qualification remain separate BPR-1811 obligations. The 17 public
build-3 assets and all protected producer images/maps/source remain immutable.
