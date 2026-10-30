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

## Narrow repair and native evidence

The observer now expands each configuration into its qualified C/C++/linker flag
keys, preserving the previous single-config order and removing duplicate names.
The strict JSON reader and exactly-one numbered configuration guard are unchanged.
The numbered identity fixture explicitly uses Ninja; the new bounded real
Ninja Multi-Config fixture observes separate Debug and Release markers, allocates
nothing locally, accepts a frozen Release-only identity and refuses a numbered
Debug+Release producer. Native CMake and Ninja are explicit test prerequisites.

Both native fixtures pass on macOS. Running the new multi-config fixture against
the exact frozen N4 CMake module fails with `duplicate JSON member: RELEASE`,
proving that the regression check distinguishes the faulty observer. Existing
source/options/toolchain/native guards remain unchanged. The earlier isolated
checkout's missing offline CPM cache refusal is preserved separately; it is not
reclassified as a release-builder pass. Exact raw commands and negative-control
receipt are external under `BPR-1820-focused-r1`. The affected Windows contract
will run after its current frozen Debug qualification vacates the serialized PC.
