---
schema: "implement-rfc.card.v1"
id: "BPR-2135"
title: "Verify reset removes old review data across file recreation"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "win", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2135--BPR-2135--23f6ed64acaf.json","mandate_revision":"linux-release-blockers-r1","source_baseline":"6efba00e1afbf874c88c7e5de2623a33528ff36f","milestone_plan":{"id":"linux-release-blockers-r1","path":"docs/rfcs/plans/linux-release-blockers-r1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"85ed1c7574ff3b7aabe1a100c5e0e257d52eeb20","evidence":{"kind":"artifact","detail":"Original frozen60 Debug inode-equality failure retained. Old numeric inode was observed only after its object lifetime ended; corrected native POSIX object anchor proves actual old-file unlink (nlink0), separate from number reuse. Source reset selectsreviewledger and unlinkat/fsync; no product data-loss/Qt/Debugtiming claim. This is a fixture lifetime-identity correction, with limits in docs/verification/BPR-2135-reset-file-identity.md."}},{"criterion":"AC2","outcome":"pass","revision":"85ed1c7574ff3b7aabe1a100c5e0e257d52eeb20","evidence":{"kind":"artifact","detail":"Finalscript4fec3110 retains every original healthy/quit/cache/log/connection/refused-copy assertion and exact timebound, original inode distinction (now anchored POSIX), and adds unique old-data sentinel/no-reset negative. App stopped before test-ownedSQL access; private-copy reads only; POSIX readfd noninherited; Windows sharing unchanged. No product/source/library/graph/app-pin/configuration edits."}},{"criterion":"AC3","outcome":"pass","revision":"85ed1c7574ff3b7aabe1a100c5e0e257d52eeb20","evidence":{"kind":"command","detail":"Final frozen-compatible e165af950/source4fec3110 fullnative LinuxRelease+Debug each0, original configuredsandbox/interpreter/ENV/workdir/watchdog against unchangedcompiled60apps. Both actual old_file_unlinked=True/previous_data_removed=True/no_reset_replacement_rejected=True/finalOK, exact producer/helper/cache/cohort/script/supportseals, normal6efclean, all4recordednativeforests empty. Earliercandidate fullpasses separatelyretained, not represented as originalreproduction. Crossplatformclosure/gateselection explicit; no unrunMac/Windows credit or successor qualification."}},{"criterion":"AC4","outcome":"pass","revision":"85ed1c7574ff3b7aabe1a100c5e0e257d52eeb20","evidence":{"kind":"command","detail":"Only own testcommits privateb9c814499->Mainfc1b985fc and privatee165af950->Main85ed1c757 plusownproof/cardDone. Canonicalprivatebaselines compatiblewithfrozen60; no foreignMainproductintake/push, R/Wmutations/cohort/sign/package/publication. Nativeowncommands terminal; PCtemporarilyserializedto2136peer forindependentfinalchecks; globalforeignplan driftpreserved. Documentation/ownedpinnedplan/syntax/diffchecks pass; finalreleaseadoption/remaininggatesrootowned."}}]
---

# Verify reset removes old review data across file recreation

Frozen60 Linux Debug profile recovery fails its Reset everything replacement
check: the ledger inode number equals the number recorded before reset.
The test releases the old file before reset; POSIX unlink/recreate may reuse
that numeric inode. Source explicitly selects and unlinks review data, but
that alone does not establish the original native failure's cause. Preserve
it and prove actual old-data removal before classifying this as a fixture bug.

## Acceptance

- AC1: Bind the original functional failure and actual reset contract; use disposable native data to distinguish data retention from inode reuse without claiming a product/Qt or Debug timing waiver.
- AC2: If fixture identity is the cause, require a unique old-data sentinel absent after the complete original public reset course, with a no-reset negative that retains it and rejects. Preserve the original course's dialog/startup/cache/connection/log/quit assertions and time bounds; no app source relaxation or unsafe live-store access.
- AC3: Verify the exact corrected course in native Release and Debug under configured containment against unchanged60 binaries where justified; retain original failure, protected producer seals, actual results and scope. Document cross-platform closure and meaningful regression controls.
- AC4: Land only owned fixture/test/issue/plan/proof paths on Main, keep unrelated work and frozen R/W/compiled inputs immutable, then return exact commits and terminal leases for named selective successor intake. If product cause is instead proved, implement and verify its minimal repair through the same owned workflow.

## Verification

The corrected fixture passed complete native Linux Release and Debug courses
with original compiled60 apps, all original public oracles and live-object
unlink/sentinel/no-reset proof. This is a fixture correction, not a product
reset or Qt fix. [Exact source and proof](../../verification/BPR-2135-reset-file-identity.md).

## Historical follow-up

BPR-2137 records the subsequent SQLite context-lifetime and missing-ledger
fixture-precondition corrections. The original Linux evidence and acceptance
records above remain tied to script4fec3110; they supply no Windows pass or
current-candidate qualification. BPR-2135 remains terminal. The final2137
script2ef08b3c separately passes full Linux/Windows Release/Debug and retained
handle positive/leak-negative controls. See the historical follow-up note in
[the identity proof](../../verification/BPR-2135-reset-file-identity.md).
