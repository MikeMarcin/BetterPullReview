---
schema: "implement-rfc.card.v1"
id: "BPR-1551"
title: "Bind declared private-library PE and signing proof"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
metadata: {"assigned_implementer":"/root/bpr1525_implementer","mandate":"authorized-release-continuation-final-gates-r3","milestone_plan":{"id":"bpr-1551-private-library-release-proof","path":"docs/rfcs/plans/bpr-1551-private-library-release-proof/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d","evidence":{"kind":"artifact","detail":"Fixed declared private-library set and selected-build path containment grant producer authority; exact bytes, DLL type and bound full map identity/address/static names are required. Native actual DLL proof824symbols passes; bad bytes, unknown identical DLL, missing/wrong map and hidden seam controls all refuse. No unknown-DLL or filename exemption.","paths":["docs/issues/_checkpoints/BPR-1551/native-proof-r1.json","docs/issues/_checkpoints/BPR-1551/SELF_REVIEW.md"]}},{"criterion":"AC2","outcome":"pass","revision":"23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d","evidence":{"kind":"artifact","detail":"Actual native MSVC SHARED_LIBRARY link emits46,203-byte full map for44,032-byte DLL; existing ASLR/DEP/CFG declaration passes. Existing Release EXE PDB/OPT/hardening and xdiff pin/dynamic linkage/source arrangement remain. Final signed DLL passes exact bound full-map proof in17-image package audit.","paths":["docs/issues/_checkpoints/BPR-1551/native-proof-r1.json","docs/issues/_checkpoints/BPR-1551/artifact-acceptance-r4.json"]}},{"criterion":"AC3","outcome":"pass","revision":"23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d","evidence":{"kind":"artifact","detail":"Maintained driver signs app/helpers/declared source-built DLL before CPack and checks exact staged bytes; offline receipt set includes all4images. Staged DLL substitution refuses before MSI signing; missing DLL receipt refuses after regenerated ordinary hashes. Actual final MSI and all4images have Valid approved publisher/timestamps, with matching inventory/source bindings.","paths":["docs/issues/_checkpoints/BPR-1551/native-proof-r1.json","docs/issues/_checkpoints/BPR-1551/artifact-acceptance-r4.json"]}},{"criterion":"AC4","outcome":"pass","revision":"23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d","evidence":{"kind":"artifact","detail":"All meaningful native bad-producer/unknown/missing-map/wrong-map/hidden-seam controls fail; actual native full-map/byte/hardening proof passes. Mac71cases69pass2explicit native skips; Windows71cases68pass3explicit platform skips, complementary Windows/Bash/POSIX coverage. Linux-native CPack fixture unexecuted/outside selected Windows release. Original4c failure and actual producer provenance retained; no runtime-source change or broad705 rerun.","paths":["docs/issues/_checkpoints/BPR-1551/native-proof-r1.json","docs/issues/_checkpoints/BPR-1551/SELF_REVIEW.md"]}},{"criterion":"AC5","outcome":"pass","revision":"23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d","evidence":{"kind":"artifact","detail":"Coordinator-only exact23447 packageR2exit0 and actual extracted-MSI auditR4exit0 supplied. Package/security mechanics pass17PEimages; MSI+all4source-built image ValidMichaelMarcin/timestamp receipts and exact hashes agree. Final source/SPDX/corresponding-source identity remains23447. Local docs/self-review/board/canonical finish completes with no PC command or artifact/tag rebinding; root owns publication/download and wider native/security/legal gaps.","paths":["docs/issues/_checkpoints/BPR-1551/artifact-acceptance-r4.json","docs/issues/_checkpoints/BPR-1551/CURRENT.md","docs/issues/_checkpoints/BPR-1551/SELF_REVIEW.md"]}}]
---

# BPR-1551: Bind declared private-library PE and signing proof

The exact signed Windows0.2.2 candidate4c118fb96693b034ba1c5c80754ccc3e88746983
fails mechanical package audit with undeclared-pe-producer for its source-built
bpr_xdiff.dll. Its native producer has no adjacent full linker map; its extracted
DLL is unsigned while all three EXEs/MSI are signed. Preserve failed originals.
BPR-1527 stays Done; this is a separate bounded follow-up under the coordinator's
explicit release-blocker amendment and sole-owner PC engineering lease.

## Acceptance

- AC1: Bind only the declared private-library package identity to its exact emitted native producer; require byte equality and full PE map identity/address/static-symbol evidence. Unknown or mismatched DLLs still fail, without filename exemptions or weaker seam scanning.
- AC2: Native MSVC source-built shared libraries emit full linker maps; retain existing hardening and binary architecture, separate LGPL linkage/source obligations and tests-disabled composition.
- AC3: The existing Windows package driver signs all declared source-built EXEs and private libraries before CPack, verifies exact staged signed bytes/publisher/timestamps, and offline manifest checks require their complete receipts. Wrong/missing/duplicate/substituted producers fail; no credentials or parallel package route is introduced.
- AC4: Meaningful bad-producer, missing/wrong-map, hidden-fixture-symbol and unknown-DLL controls fail; current portable script/fixture checks and actual native compiled-library PE/map audit pass. Preserve original package failure and source provenance; no application705 rerun for verifier-only changes absent changed runtime behavior.
- AC5: Hand off committed source for coordinator-only rebuild/sign into a new output, obtain actual exact-source expanded-package audit/signature coverage, complete documentation/self-review/canonical upkeep and leave commands quiescent. Prior signed candidate remains immutable evidence; wider release/native/security/legal qualification stays explicit.

## Ownership

Source worktree codex/v021-windows-gate-fixes adopts exact4c118 release metadata
plus the prior docs-only333cc904a note. Main32e9bcc0b reserves only this ID;
allocation receipt/counter are adopted without unrelated main changes. Same
rfc-implementer/gpt-6.1-sol/xhigh, no subagents. Root transfers PC audit/build slot;
this owner never packages, signs, publishes or changes credentials/settings.

## Execution record

Initial read-only native inventory is quiet. Producer C:/Users/mikem/bpr-v021
is clean4c118; actual44,032-byte bpr_xdiff.dll has no .map, and extracted MSI
DLL Authenticode status is NotSigned. Three EXE/MSI signatures remain their
original successful observations; no current package pass is inferred.


Actual native R1 on committed23447 passes full DLL/map proof (824symbols), all
five adversarial controls and68of71script cases with3explicit platform skips.
Portable Mac69of71passes complement Windows/Bash/POSIX coverage; Linux-native
CPack remains unexecuted and no Linux release is selected. Fixed declaration,
byte/map/type/symbol checks and all4source-built signing receipts are retained.

Coordinator-supplied finalR2/R4 evidence now closes AC5: actual new MSI extraction,
all4source-built images plus MSI ValidMichaelMarcin/timestamped, and both package
and security-mechanics audits pass on17PEimages with bound xdiff full-map proof.
[Final artifact acceptance](../_checkpoints/BPR-1551/artifact-acceptance-r4.json)
and [checkpoint](../_checkpoints/BPR-1551/CURRENT.md) preserve exact hashes,
original failures, skips and remaining qualification limits. Canonical Done is
a docs-only development commit after frozenproducer23447; it never rebinds the
artifact/tag or triggers another package build. Root retains publication/download
verification and the raw31scan/security/legal/native-account/vault/crash gaps.
