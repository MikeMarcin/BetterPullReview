---
schema: "implement-rfc.card.v1"
id: "BPR-1624"
title: "Validate the Insights budget fixture through the full security contract"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"f6ba8f43e4477f60941dd24cdac4450ca68de911","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8f73f8cdd","evidence":{"kind":"command","detail":"Full 18-test github_contract_fixture_security_tests.py passes after adding exact Insights identity and populated response field shapes. All eleven fixture files traverse existing validators; no fixture or policy assertion changed."}},{"criterion":"AC2","outcome":"pass","revision":"8f73f8cdd","evidence":{"kind":"artifact","path":".mask/sessions/release-v0.3.0/fixture-security-negative-controls.json","detail":"Disposable Insights viewer extra environment field rejected while byte scan stays green; separate synthetic forbidden-byte canary rejected by exact-tree byte check. Both expected failures observed, temp tree removed."}},{"criterion":"AC3","outcome":"pass","revision":"8f73f8cdd","evidence":{"kind":"command","detail":"DOCUMENTATION_OK; all67 documentation tests pass; git diff --check passes. Self-review confirms only table entry executable change; fuzz recipe updated to actual production parser/SHA composition with thresholds unchanged. Native reruns and sanitizer execution remain coordinator-owned."}}]
---

# BPR-1624: Validate the Insights budget fixture through the full security contract

The v0.3.0 macOS normal gate at `f6ba8f43e4477f60941dd24cdac4450ca68de911` fails `github_contract_fixture_security`: the committed `insights-budget-v1` manifest and ten responses are absent from the validator's exact inventory. The coordinator authorized this bounded source repair while native baseline gates continue on their frozen revision.

## Acceptance

- AC1: Bind `insights-budget-v1` to the exact identity and response shapes its committed bytes carry, through the existing fixture table. All eleven files traverse the existing byte, decoded JSON, field/artifact and read-only manifest checks.
- AC2: Run the focused fixture-security contract and a negative control that adds a forbidden field to the Insights fixture in a disposable copy and observes rejection. Preserve the exact-tree requirement, security policy, fixture bytes and test skips.
- AC3: Pass documentation verification and diff checks, self-review and commit explicit paths, and hand the candidate plus focused evidence to the coordinator for matching native reruns.

The coordinator also authorized a bounded documentation correction after the
gate exposed a stale direct-fuzz recipe. The release policy must name the
current diagnostic parser and production SHA-256 composition, preserve all
existing fuzz limits and leave the actual sanitizer runs with the coordinator.

## Boundaries

This card repairs a fixture-security test inventory and the stale source references in the release policy's existing fuzz recipe. Product behavior, dependencies, signing and credential handling are unchanged. The coordinator owns native producers, deployment of locked MASK skills in their clean checkouts, full release gates, signing, packaging and publication. Missing `board_model` or installed skill scripts in an unregistered producer is an environment prerequisite, not a validator exception. RFC0038 qualification gaps and all raw native gate failures remain explicit.

The [release engineering ledger](../../verification/release-v030-engineering.md) records the result. The ignored durable repair return is `.mask/sessions/release-v0.3.0/repair-handoff.json`.
