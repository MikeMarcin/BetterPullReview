---
schema: "implement-rfc.card.v1"
id: "BPR-2125"
title: "An interrupted ownership-journal migration can delete a retained agent workspace"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2125--BPR-2125--2753119c9f4e.json","source_baseline":"363b2d77d542fd0d7fc8e481d7bd147d52625c3d","mandate_revision":"bpr2125-interrupted-migration-safety-r1","related_cards":["BPR-2118"],"milestone_plan":{"id":"bpr-2125-interrupted-migration-safety","path":"docs/rfcs/plans/bpr-2125-interrupted-migration-safety/milestone-plan.json"}}
acceptance: []
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

## Proposed correction

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
