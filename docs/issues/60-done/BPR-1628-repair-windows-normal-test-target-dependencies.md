---
schema: "implement-rfc.card.v1"
id: "BPR-1628"
title: "Repair Windows normal-test target dependencies"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"aaff2f11419257be2c4c4de5646e3465cf4bbf8c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"613f33524f92e844e8996126f86c5f64e54b1103","evidence":{"kind":"observation","detail":"Target-only CNG object stub scope, explicit Platform/advapi32 and MSVC bigobj dependencies compile all seven affected actual native targets at9b901; no product policy changes."}},{"criterion":"AC2","outcome":"pass","revision":"613f33524f92e844e8996126f86c5f64e54b1103","evidence":{"kind":"observation","detail":"All focused CNG modes,Git Insights,Help,canary and workbench pass; SQLite actual direct and measured ancestor denial retained; corrected row18dff passes0.34s and mandatory native manifest613f passes1.24s. Raw failures preserved."}},{"criterion":"AC3","outcome":"pass","revision":"613f33524f92e844e8996126f86c5f64e54b1103","evidence":{"kind":"observation","detail":"Explicit commits and self-review complete; documentation/diff checks pass; owned native processes/tasks/probes and exact fixture roots reconciled, PC slot returned to coordinator. Full gates remain pending outside this card."}}]
---

# BPR-1628: Repair Windows normal-test target dependencies

The coordinator added this bounded repair after the frozen `f6ba8f43e4477f60941dd24cdac4450ca68de911` native normal gate supplied the actual failure. No signing, credentials, remote publication, test skip, security-policy relaxation or new UX is authorized by this card.

## Acceptance

- AC1: Isolate the existing CNG API stub include scope from the automatically injected native quiet-CRT unit, give both Insights storage tests their explicit Windows scratch-root dependencies, and enable /bigobj only for the oversized MSVC workbench test target.
- AC2: Preserve the actual native test assertions, capture the native baseline failure, rebuild affected targets and run relevant focused native contracts against the exact committed repair candidate, retaining raw failures and all existing assertions/CRT/private-root behavior.
- AC3: Self-review and pass documentation/diff checks, commit explicit paths, reconcile native commands and hand remaining full gates to the coordinator.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` bind commands, candidates and pending native/release obligations.

## Measured native containment correction

The coordinator extended this card after the repaired targets built: the actual
Insights SQLite test passes directly at `9b901a0a83d1016d5e2da3795b1e7fa5249c4b85`
but fails contained before creating its lifetime store. A probe compiling the
actual scratch descriptor and native admission header confirms canonical root
success in both contexts; the first ancestor `CreateFileW` with
`FILE_READ_ATTRIBUTES | READ_CONTROL` succeeds directly but receives Win32 5
inside the AppContainer, and `validate_ancestor_chain` reports
`invalid_permissions`. Both probes remove their scratch root. This supplies the
existing finite `ancestor-control-denied` reason, not an inferred platform skip.
Only `sqlite_insights_storage` changes to the established Windows-uncontained
registration with its exact verifier pin. The row still runs all assertions;
macOS/Linux containment, the passing Git Insights row and all other registrations
are unchanged. The mandatory manifest check and corrected native row must pass.
