---
schema: "implement-rfc.card.v1"
id: "BPR-1320"
title: "Qualify the affected Git installation and exact-file recovery"
status: "In Progress"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1319"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
---

# Qualify the affected Git installation and exact-file recovery

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Direct installed-tool acquisition, confinement, bounds, credential isolation and exact-object evidence qualifies only a precisely identified installation; no arbitrary range gate.
- C2: Composed multi-file GHES exact source, retry/reopen and persisted failed-state recovery pass; stale head/account/host/selection and cancellation cannot publish wrong content; genuine refusal is typed/actionable.

## Owner support-policy amendment and evidence boundary

The owner rejects exact Git executable-version/banner equality. Required
operation admission uses Git 2.49+ because exact revision cloning needs
`clone --revision`, then a bounded credential-free local clone-help probe.
Successful admission still performs confined acquisition, exact object and
pack verification and identity checks; an unsupported tool and a failed probe
reach the core and Qt as distinct actionable reasons. The CLI opt-in, account,
host, request identity, cancellation and supervised store bounds remain.

The direct native read-only proof identifies `/usr/bin/git` 2.50.1 (Apple
Git-155), canonical Homebrew gh 2.101.0 and macOS 26.6.2 build 25G83. It proves
exact commit/tree/blob/pack material, denied outside writes and cleanup for
that installation. The owner reports Git 2.54.0 (Apple Git-157); its configured
executable paths and source/build correspondence remain unavailable. Local
Git-155 evidence does not qualify that installation or private GHES access.
Original C1/C2 remain intact; actual affected installation and rendered PR
recovery obligations are not replaced by fixture or capability-policy evidence.
