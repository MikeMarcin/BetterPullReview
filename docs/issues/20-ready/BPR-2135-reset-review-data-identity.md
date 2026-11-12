---
schema: "implement-rfc.card.v1"
id: "BPR-2135"
title: "Verify reset removes old review data across file recreation"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "win", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2135--BPR-2135--23f6ed64acaf.json","mandate_revision":"linux-release-blockers-r1","source_baseline":"6efba00e1afbf874c88c7e5de2623a33528ff36f","milestone_plan":{"id":"linux-release-blockers-r1","path":"docs/rfcs/plans/linux-release-blockers-r1/milestone-plan.json"}}
acceptance: []
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
