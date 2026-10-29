---
schema: "implement-rfc.card.v1"
id: "BPR-1812"
title: "Accept the observed stable tag and numbered MSI floor"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Accept the observed stable tag and numbered MSI floor

The 0.4.1 allocator refuses the actual preceding stable observation: bare tag `0.4.0`, release 428060158, with `BetterPullReview-0.4.0-build.1-windows-x64.msi`. Existing validation accepts only a v-prefixed tag and derives the filename directly from that tag. Fresh native observation confirms MSI ProductVersion `0.4.1`, distinct ProductCode, retained UpgradeCode, per-machine scope and the exact published installer digest. No new release number was reserved before this refusal.

## Acceptance

- AC1: Admit the actual base stable tag with its correctly bound numbered installer identity and the existing legacy/numbered RC floors. Preserve exact release/digest fields, number bounds, MSI/marketing ordering, ProductCode safeguards and historical rollback/replacement refusal. Reject mismatched tag/version, installer/version/build and MSI mapping.
- AC2: Exercise direct allocation/freeze with the published stable floor and meaningful negative controls, including native-floor history rollback and replacement. Reconcile the actual fresh MSI/public evidence and complete focused tests/documentation before retrying the real allocator. No unsigned/native installation or scanner acceptance is inferred.
