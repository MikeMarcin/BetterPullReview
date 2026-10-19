---
schema: "implement-rfc.card.v1"
id: "BPR-1628"
title: "Repair Windows normal-test target dependencies"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"aaff2f11419257be2c4c4de5646e3465cf4bbf8c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1628: Repair Windows normal-test target dependencies

The coordinator added this bounded repair after the frozen `f6ba8f43e4477f60941dd24cdac4450ca68de911` native normal gate supplied the actual failure. No signing, credentials, remote publication, test skip, security-policy relaxation or new UX is authorized by this card.

## Acceptance

- AC1: Isolate the existing CNG API stub include scope from the automatically injected native quiet-CRT unit, give both Insights storage tests their explicit Windows scratch-root dependencies, and enable /bigobj only for the oversized MSVC workbench test target.
- AC2: Capture the native baseline failure, rebuild affected targets and run relevant focused native contracts against the exact committed repair candidate, retaining raw failures and all existing assertions/CRT/private-root behavior.
- AC3: Self-review and pass documentation/diff checks, commit explicit paths, reconcile native commands and hand remaining full gates to the coordinator.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` bind commands, candidates and pending native/release obligations.
