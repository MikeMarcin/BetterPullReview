---
schema: "implement-rfc.card.v1"
id: "BPR-2125"
title: "An interrupted ownership-journal migration can delete a retained agent workspace"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2125--BPR-2125--2753119c9f4e.json","source_baseline":"363b2d77d542fd0d7fc8e481d7bd147d52625c3d","mandate_revision":"bpr2125-interrupted-migration-safety-r1","related_cards":["BPR-2118"],"milestone_plan":{"id":"bpr-2125-interrupted-migration-safety","path":"docs/rfcs/plans/bpr-2125-interrupted-migration-safety/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"67c3e67e53c37b4f3afc8e1d89368195310eeee8","evidence":{"kind":"artifact","detail":"Root-reviewed forward protocol stops before body mutation unless marker write+flush succeeded, and before JSON publication unless full body/truncate/flush succeeded. Seventeen native actual-writer/crash receipts preserve retained edits and exact journal inodes. Ordinary parser/phase rules are unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"67c3e67e53c37b4f3afc8e1d89368195310eeee8","evidence":{"kind":"command","detail":"Stock network-isolated agent_workspace_ownership passes eight process-death checkpoints, seven I/O failure transitions, actual short pwrite failure/completion, exact old2690/full3350/endPhase3=2653/cut2691, and operation-prefix assertions proving later I/O unreached. Original unsafe native before-images remain separate."}},{"criterion":"AC3","outcome":"pass","revision":"67c3e67e53c37b4f3afc8e1d89368195310eeee8","evidence":{"kind":"command","detail":"Focused native git_process_contract, git_cache_contract and application_agent_invocation_host pass; retained legacy reattach and returned durable guard pass. Complete v2 retained state stays exact and both v1/v2 disposable phase3 controls clean as before; existing ownership tamper/refusal courses pass."}},{"criterion":"AC4","outcome":"pass","revision":"67c3e67e53c37b4f3afc8e1d89368195310eeee8","evidence":{"kind":"command","detail":"Private candidate documentation/architecture/conditional/app-inventory adversarial/clang-format22/clang-tidy22 pass; four focused Mac CTest rows each childexit0. Normal engine object has no migration-fault symbol. Windows/Linux native and final tests-off release checks remain explicitly pending. Canonical docs pass; unrelated whole-main app digest drift is recorded and not resealed."}},{"criterion":"AC5","outcome":"pass","revision":"67c3e67e53c37b4f3afc8e1d89368195310eeee8","evidence":{"kind":"artifact","detail":"Canonical metadata b745f1299, source63b70be81d6476dee0bbd0b0d6934b1e67b691ae and receipt1cb6469a9b0ce594910c4b2a9809d2a862fff0ab land only owned paths over foreign5ee3252. Current main counter2128 and foreign work preserved. Frozen producer unchanged; release staging writes belong only to coordinator. No cohort/sign/package/publication or remote push."}}]
---

# An interrupted ownership-journal migration can delete a retained agent workspace

[[BPR-2118]] converts a validated version1 ownership journal to version2 in place.
The version2 APFS UUID representation is longer. A partial overwrite that extends
past the original EOF can leave complete version2 phases0,1,2,3 and an incomplete
phase7. Recovery discards the incomplete tail and treats phase3 as disposable,
recursively deleting a tree that a retained conversation still owns.

A bounded native diagnostic of the unmodified implementation confirmed this:
old record2690 bytes; complete replacement3350; phase3 ends at2653; writing the
first2691 replacement bytes through the same journal inode leaves a parseable
phase3 prefix. Normal recovery returned `recovered=1 live=0` and deleted the
disposable retained workspace and its test file. A complete-record control
returned `recovered=0 live=1` and preserved both. This modeled an interrupted
write in private fixtures; no actual crash or remount was performed.

## Correction

Keep byte0 deliberately invalid throughout migration. Flush an invalid marker
before writing any other byte; write the replacement body from offset1, truncate
and flush it; restore the opening JSON byte only after the body is durable, then
flush the commit. The same inode and existing exclusive lock remain authoritative.
A pending marker refuses explicitly. Ordinary journal append/recovery and phase3
temporary-workspace cleanup retain their existing acceptance rules.

The old bare version2 prefix remains indistinguishable from a legitimate
temporary phase3 journal. Acceptance therefore proves that the corrected migration
writer cannot produce that state; it does not weaken ordinary cleanup by refusing
all valid partial append tails.

## Acceptance

- AC1: The migration writer cannot expose a parseable cleanup-eligible phase2/3 prefix from a retained phase7 chain after any short write, interrupted write, flush failure or truncate failure. It preserves the journal inode, exclusive lock, original authority, workspace and retained edits; the next reader either sees the complete migration or refuses safely.
- AC2: Native fixture regressions exercise actual-writer boundaries around invalidation, body writes, flushes, truncate and final commit, including the original2690/3350/cut2691 counterexample and an actual pending-marker partial body. Bind each expected retained state to a valid full phase7 before-image and preserve fault receipts.
- AC3: Complete legacy migration and retained reattachment pass; current version2 retained records remain unchanged; ordinary temporary creation/phase3 cleanup still removes only its intended disposable tree. Existing strict marker, inode, volume, owner, link, token, phase, scope, authority and pinned-object checks remain intact.
- AC4: Focused macOS ownership/process/cache/host controls and source/architecture/format/documentation gates pass. Preserve Windows/Linux identity and I/O branches; their native qualification remains an explicit release obligation.
- AC5: Land only this issue's source, tests, documentation and tracker paths on main and return the exact private-to-canonical commit map. Keep frozen703 and release successor363 unchanged; no cohort allocation, signing, packaging or publication.

## Verification

[Bound native fault matrix and source contribution](../../verification/BPR-2125-interrupted-migration.md).
Windows/Linux native release qualification remains pending; this card supplies focused Mac evidence only.
