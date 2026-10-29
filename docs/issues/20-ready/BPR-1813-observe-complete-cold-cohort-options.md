---
schema: "implement-rfc.card.v1"
id: "BPR-1813"
title: "Observe complete cohort options on a cold configure"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Observe complete cohort options on a cold configure

Cold native configuration wrote its release observation before core declared `BPR_CORE_SHA256_PORTABLE`. A warm configure correctly reports its effective `OFF`, so frozen 0.4.1 build 2 was refused on Mac and Windows before signing or package creation. The raw failed commands, source `c674b7038` and N2 receipt/cohort remain immutable; the changed complete configuration requires a new number.

## Acceptance

- AC1: Declare the existing portable SHA-256 option before release observation on cold configure without changing its default, selection, core dependency ownership or strict cohort comparison. A real cold and warm configuration exposes the same complete option set and value.
- AC2: Add a durable regression and verify the actual source through cold/warm CMake, focused release-build/core-boundary checks and documentation. Gather complete actual native producer observations for the successor; preserve failed N2, actual exits, source/configuration binding and monotone allocation.
