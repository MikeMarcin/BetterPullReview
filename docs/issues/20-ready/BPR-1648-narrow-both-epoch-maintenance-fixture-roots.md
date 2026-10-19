---
schema: "implement-rfc.card.v1"
id: "BPR-1648"
title: "Narrow both epoch maintenance fixture roots"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/git_contract/epoch_maintenance_integration_tests.cpp"]}
---

# BPR-1648: Narrow both epoch maintenance fixture roots

The native epoch maintenance fixture narrows catalog with make_storage_root_owner_private but uses POSIX permissions only on git. Actual Windows is_owner_private(git,true) is false before Insights storage construction.

## Acceptance

- AC1: Apply the existing owner-private helper to the fixture git root as well as catalog, preserving production storage permission checks, lifetime, pruning, deletion and byte totals.
- AC2: Rebuild and run the complete native epoch_maintenance_integration course and retain the original direct Insights review store unavailable failure and measured root predicate.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.
