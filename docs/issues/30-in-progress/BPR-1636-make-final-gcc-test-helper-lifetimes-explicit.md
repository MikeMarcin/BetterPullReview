---
schema: "implement-rfc.card.v1"
id: "BPR-1636"
title: "Make final GCC test helper lifetimes explicit"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"3ca343cbeb252b476b620a340cd821802ab08539","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1636: Make final GCC test helper lifetimes explicit

The coordinator's native Ubuntu keep-going build supplies exactly two failed
translation units: three macOS Option-key helpers are unused off Apple, and
GCC15 diagnoses a temporary expected membership vector in the provider test's
compound equality assertion. All other available build actions succeed.

## Acceptance

- AC1: Scope only the three actual macOS helpers to their existing Apple-only caller and name the provider expectation vector, preserving complete Option coverage and exact membership/count equality without warning overrides.
- AC2: Retain original raw failures, compile/run the affected native tests and scoped tidy, self-review/document/commit explicit paths, and hand full native gates to the coordinator.
