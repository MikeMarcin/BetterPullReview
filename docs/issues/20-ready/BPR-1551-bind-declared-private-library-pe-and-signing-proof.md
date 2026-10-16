---
schema: "implement-rfc.card.v1"
id: "BPR-1551"
title: "Bind declared private-library PE and signing proof"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
metadata: {"assigned_implementer":"/root/bpr1525_implementer","mandate":"authorized-release-continuation-final-gates-r3","milestone_plan":{"id":"bpr-1551-private-library-release-proof","path":"docs/rfcs/plans/bpr-1551-private-library-release-proof/milestone-plan.json"}}
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
