---
schema: "implement-rfc.card.v1"
id: "BPR-1649"
title: "State wide tray layout before label assertions"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/adapters_qt/workbench_window_tests.cpp"]}
---

# BPR-1649: State wide tray layout before label assertions

The actual327px tray correctly presents four icon-only tabs below the existing340px product threshold. The fixture asserts full text without specifying a wide host layout; icons, accessibility names, selection and visibility already match.

## Acceptance

- AC1: Supply an explicit wide host layout in the label-bearing tray fixture. Preserve the compact product threshold and every original label, count, focus, privacy, action and accessibility assertion; do not force a widget minimum or alter the product UI.
- AC2: Rebuild and run current-file unread badge plus all three complete workbench scale rows, retaining the original predicate/geometry evidence and existing narrow-layout coverage.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.
