---
schema: "implement-rfc.card.v1"
id: "BPR-1623"
title: "Prepare the coordinated v0.3.0 release source"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_engineering","mandate":"release-v030-coordinated-r1","baseline":"a426db419d393895020ab961509df46aba94ee46","milestone_plan":{"id":"release-v030-engineering","path":"docs/rfcs/plans/release-v030-engineering/milestone-plan.json"}}
---

# BPR-1623: Prepare the coordinated v0.3.0 release source

The owner requested a full coordinated stable 0.3.0 release for macOS,
Windows x64 and Ubuntu 26.04. Full agent integrations and local automation
remain selected. The release coordinator owns native producer dispatch, normal
native gates, signing, packaging and publication; the assigned implementer owns
the committed source preparation and any later explicitly scoped gate repairs.

## Acceptance

- AC1: Set the CMake project version to 0.3.0 and provide user-facing release notes grounded in changes since v0.2.2, with the selected stable release and platform set explicit.
- AC2: Preserve RFC-BPR-0038's native, vault, crash/privacy, vulnerability, exact-artifact trust and source-distribution obligations; prior decisions and configured test skips retain their actual scope.
- AC3: Pass the documentation gate and documentation tests, self-review the explicit diff, commit the release source, and hand back its full identity, changed paths, pending native gates and quiescent command ownership.

## Boundaries

This card supplies source readiness, not package or qualification acceptance.
Do not run full native builds/tests here while the coordinator owns the producer
slots. Do not stop the pre-existing Windows ctest PID 104444, whose owner is
unknown. No remote production or publication authority is delegated. Preserve
unrelated writers and historical Done cards. Later gate failures require an
explicit coordinator handoff and their own bounded repair acceptance.

The engineering ledger is [v0.3.0 release engineering](../../verification/release-v030-engineering.md).
The ignored durable return is `.mask/sessions/release-v0.3.0/engineering-handoff.json`.
