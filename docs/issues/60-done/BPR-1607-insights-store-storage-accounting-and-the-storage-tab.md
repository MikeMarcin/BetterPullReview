---
schema: "implement-rfc.card.v1"
id: "BPR-1607"
title: "Insights store, storage accounting and the Storage tab"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1606"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1607:store","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Private state/v1/insights.sqlite3 initializes at first launch and survives restart. Existing profile-maintenance purge removes it and its sidecars; only Reset everything among Troubleshooting resets removes statistics. Clear local diagnostics and cache maintenance preserve it. Native SQLite maintenance/reset and core reset tests pass. Integration revision 4a89314cc5d4191365196e600c2f59e6546da200: 10/10 focused checks pass in 24.59 seconds; see RFC-BPR-0086 ledger."}},{"criterion":"BPR-1607:accounting","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Independent filesystem walk matches exact store file lengths including WAL/SHM; Git inventory reports exact bytes per repository without a Git executable, and refuses links. All state/v1 stores, Markdown images, Review Cycles and publication objects, logs and Insights itself are included once, with existing budgets. sqlite_insights_storage and git_insights_storage pass. Integration revision 4a89314cc5d4191365196e600c2f59e6546da200: 10/10 focused checks pass in 24.59 seconds; see RFC-BPR-0086 ledger."}},{"criterion":"BPR-1607:measurement","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Host checks and composed public path pass: measurement is off the decision/UI lane, one pending snapshot, at most one measurement per 60 seconds, only Summary or Storage; hidden timers stop. Manage cached repositories reaches existing controls. No provider/Git/process refresh capability is composed. Integration revision 4a89314cc5d4191365196e600c2f59e6546da200: 10/10 focused checks pass in 24.59 seconds; see RFC-BPR-0086 ledger."}},{"criterion":"BPR-1607:export","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Actual anonymized maintenance preview includes only numeric byte aggregates; identifying export adds no Insights section. Storage export privacy canaries exclude repository/account/PR/path/body. Native export/frozen-descriptor tests pass. Later statistics export privacy remains a later milestone obligation. Integration revision 4a89314cc5d4191365196e600c2f59e6546da200: 10/10 focused checks pass in 24.59 seconds; see RFC-BPR-0086 ledger."}}]
---

# Insights store, storage accounting and the Storage tab

## Scope and acceptance

Implement RFC-BPR-0086 "Storage", the store part of "Data, privacy and
lifetime", AC05, and AC11's store classification.

- `insights.sqlite3` in `state/v1`, classified as a profile state store, so
  profile purge removes it.
- Byte accounting for every store, the Git cache by repository (from the
  adapter's existing inventory), logs, Markdown images and Review Cycles,
  against their budgets. Measured at most once per 60 s, only while Summary
  or Storage is visible.
- Manage cached repositories opens the existing epoch and cache controls (D8).
- The anonymized export gains an Insights section of byte sizes only.

## Execution record

Implemented and verified with BPR-1606 under the authored `window-and-storage`
assignment. The owner accepted the corrected shell on 2026-10-18, session
`e51e8b870c12f3d41b3f243b2891758a`, event 8; event 9 handed back. BPR-1606
is Done, so this storage slice was promoted and claimed. See the first-milestone
[verification ledger](../../verification/RFC-BPR-0086.md) and
[continuation](../_checkpoints/insights-window-v1/continuation.md).
