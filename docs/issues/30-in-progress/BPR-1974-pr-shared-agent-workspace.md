---
schema: "implement-rfc.card.v1"
id: "BPR-1974"
title: "Share one PR workspace across agent conversations"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1972"]
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"mandate_revision":"pr-shared-workspace-critical-r2","reservation":"docs/issues/_reservations/BPR-1974--BPR-1974--b363004b8b8b.json","milestone_plan":{"id":"bpr-1974-shared-workspace","path":"docs/rfcs/plans/bpr-1974-shared-workspace/milestone-plan.json"},"original_mandate":"pr-shared-workspace-r1","required_review":"independent-critical"}
---

# Share one PR workspace across agent conversations

The owner says: "It's seemingly making a separate workspace for every tour stop. That doesn't make sense. The PR only needs one workspace across all tour side chats and agent tabs and even agents. Sure there can be different sessions but the source of truth is the files and git which is shared."

Mandate `pr-shared-workspace-r1` amends RFC-BPR-0096's per-generation mutable tree and replay-tree isolation. One workspace is shared for exact authority plus base/head; old revision trees stay preserved for explicit old-revision continuation. No cross-account/repository/PR sharing or silent checkout/reset/merge is authorized.

## Acceptance

- AC1. Agent-tab, distinct tour stops/threads, agents/providers, New session and transcript replay share one exact PR/revision workspace while retaining independent conversation identities and native handles across reopen.

- AC2. Authority and base/head isolation, nonblocking per-workspace turn exclusion across hosts, cancellation and failure preserve files and exact ownership; no turn resets, cleans or replaces a shared tree.

- AC3. Legacy sessions adopt their existing tree only when no shared tree exists; a conflicting legacy tree refuses native resume with files/transcript preserved and an explicit New session path. Git commits in the shared tree survive reuse while pinned source objects and ownership remain validated.

- AC4. Focused host, SQLite, real-Git and public native/tour/replay composition checks distinguish shared workdir behavior. Contracts, migration limits, evidence and tracker state are committed with commands quiescent.

## Required acceptance amendment, 2026-11-09

The user requires `rfc-review-critical` before finish. Mandate amendment
`pr-shared-workspace-critical-r2` preserves every r1 implementation obligation
and adds independent acceptance. Product remains frozen at `d8b79055c` pending
review by coordinator-launched `/root/shared_workspace_critical`; source evidence
is not a self-certified substitute.

- AC5. An independent critical review of the exact shared-workspace candidate is
  supplied and every actionable finding is reconciled before final acceptance.
