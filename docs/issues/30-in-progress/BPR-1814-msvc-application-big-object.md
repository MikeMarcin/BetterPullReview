---
schema: "implement-rfc.card.v1"
id: "BPR-1814"
title: "Compile the application host as a large MSVC object"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Compile the application host as a large MSVC object

The fresh Windows standard Debug compilation barrier for frozen 0.4.1 build 3
failed with MSVC C1128 in `epoch_application_host.cpp`: the COFF object exceeded
its section limit. The same source's complete Release compilation passed. The
barrier stopped before Ubuntu compilation or any long qualification suite.
Build 3's public Mac and Windows assets remain immutable and their actual
verification remains recorded; this correction requires a new candidate number.

## Acceptance

- AC1: Enable MSVC's large-object format privately on the owning application
  target, following the existing composition-target treatment. Preserve normal
  Debug symbols, optimization, runtime checks, warning and hardening flags;
  leave non-MSVC compilation and downstream public compile requirements intact.
- AC2: Retain the original C1128 result and demonstrate the corrected native
  target and complete Debug graph compile. Inspect actual generated/native
  arguments and run focused source/documentation checks. Complete the unreached
  Ubuntu Release, Debug and distribution compilation before allocating the
  successor, with an explicit bridge for unaffected Mac evidence and all prior
  signed producer images, maps, source and configuration preserved.

## Evidence and scope

The original terminal result is in external
`pc-compile-barrier-r1/receipt.json` on the approved PC, with child `17800`
exiting 1 at `2026-10-29T18:15:43.900967Z`. It receives no passing credit.
The correction changes the compiler's object format capacity, not application
behavior or qualification policy. New functional failures or hangs still require
investigation; all matching release waivers retain their exact scope.
