---
schema: "implement-rfc.card.v1"
id: "BPR-1820"
title: "Observe local multi-config builds without malformed JSON"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"712460774c52dad86eb7ed5f8ef62c3b4fea1963","evidence":{"kind":"observation","detail":"Strict complete single/multi-config observations and one-configuration numbered refusal preserved; exact old module duplicate-JSON negative control retained.","externalState":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029"}},{"criterion":"AC2","outcome":"pass","revision":"712460774c52dad86eb7ed5f8ef62c3b4fea1963","evidence":{"kind":"observation","detail":"Mac real native identity fixtures PASS; Windows18 enumerated17executed/1explicitBash-onlyskip including actualmulti-configPASS; Ubuntu18executedPASS at712. Actual current3producerobservations unchanged flags/toolchain. No oldproducer relabel.","externalState":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029"}}]
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


## Native repair acceptance, 2026-10-30

The exact current source passes the complete native release-build contract on
Ubuntu: all 18 tests execute, including the real multi-configuration observation
and numbered-producer rejection controls. Windows enumerates the same 18,
executes 17 successfully and explicitly skips its Bash-only publisher-shell
course; the real native multi-config fixture executes successfully. The two
Mac native identity fixtures and their original pre-fix duplicate-member
negative control remain valid for these unchanged module/test inputs.

The receipts are external under `BPR-1820-focused-r1`,
`windows-candidate-prerequisites-r3-local` and
`successor-ubuntu-prerequisites-r2-local`. Actual current distribution
observations on all three producers retain their original flags and toolchains,
with 15 distribution options distinct from 23 tests-on options. These are
configure observations and focused repair acceptance; full release qualification
and a new numbered producer belong to BPR-1811. N4 and N6 identities, original
failures and configured platform skips remain unchanged.
