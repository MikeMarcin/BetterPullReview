---
schema: "implement-rfc.card.v1"
id: "BPR-1820"
title: "Observe local multi-config builds without malformed JSON"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Observe local multi-config builds without malformed JSON

Frozen build-4 Windows Release `native_release_contract` fails in the real CMake
cohort fixture: the default Visual Studio generator exposes multiple
configurations, whose semicolon list becomes repeated raw `RELEASE` JSON members
in the observation settings. The strict duplicate-member parser correctly
refuses it. The later WinError32 cleanup failure is also retained. Numbered
producers already require exactly one selected native configuration.

## Acceptance

- AC1: Emit complete, strict, unambiguous local multi-config observation settings
  while preserving the exactly-one-configuration numbered-producer refusal and
  every source/options/toolchain/native identity guard. Make the numbered unit
  fixture explicitly select its supported single configuration instead of relying
  on the Windows generator default. Preserve the original native failures.
- AC2: Prove single-config behavior, valid local multi-config settings and the
  numbered multi-config negative control through the real bounded CMake module
  and strict public reader. Run the affected release contracts on exact source;
  do not relabel N4 or repeat unchanged full builds. Any producer promotion needs
  the successor cohort.
