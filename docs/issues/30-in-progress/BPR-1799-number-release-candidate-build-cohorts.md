---
schema: "implement-rfc.card.v1"
id: "BPR-1799"
title: "Number release candidate build cohorts"
status: "In Progress"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0035"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "owner-release-build-number-20261028", "milestone_plan": {"id": "release-candidate-build-numbering", "path": "docs/rfcs/plans/release-candidate-build-numbering/milestone-plan.json"}}
---

# Number release candidate build cohorts

The owner requested a sequential, product-specific build number for the
publish-release workflow. Ordinary local builds do not allocate numbers.
One candidate cohort carries the same number on every platform. An unchanged
retry retains its allocation; changed source or declared candidate inputs
requires another identity. This is BPR-only work.

## Acceptance

- AC1: A durable, serialized allocation records source and complete declared
  cross-platform configuration inputs, survives interruption, prevents reuse
  across different inputs, and reuses an unchanged candidate on retry. The
  source binding avoids embedding a commit's own hash in that commit.
- AC2: Numbered native builds expose the marketing version and cohort number
  in About, copied version information and diagnostics. macOS bundle, Windows
  image/MSI and Linux package identities use valid, checked native mappings.
  Local builds remain unnumbered and consume nothing.
- AC3: Binary, corresponding source, SPDX, provenance and release manifests
  bind one exact source/configuration/cohort. Canonical download names include
  the build number. The guarded publisher rejects mixed cohorts and occupied
  releases; the legacy updater ZIP alias, when emitted, is byte-identical.
- AC4: The existing stable updater and persisted settings remain compatible
  with base-version tags. Focused public-path, allocation/recovery, package,
  publisher and native identity proofs cover positive and misuse cases.
  Documentation and actual remaining native/security gates stay explicit.

## Design and publication boundary

See [the design](../../development/release-build-identity.md). The already
published `v0.4.0` tag and its N144de source/assets remain immutable. The corrected
source needs an owner-selected next semantic version before publication; this
card does not select it. The pending AgentSkills source publication remains a
separate authorization. BPR-1797 qualification continues with its original
reports, waivers and native command ownership.

## Implementation checkpoint, 2026-10-29 UTC

The allocation/freeze slice is committed at `2ef1011dd`. The integrated product
slice now projects the cohort through CMake, About and Copy version info, local
diagnostics, native mappings, package/source/SPDX provenance, canonical assets
and the guarded publisher. It retains the exact legacy updater ZIP alias and
base-version ordering. The required frontend-design skill was loaded; About
keeps its existing native palette, fonts and layout.

Focused evidence so far: ten allocation/process/recovery/public-command cases;
three actual About composition/1×/2× courses; diagnostics host and native macOS
ZIP staging courses; 25 source/publication controls; 45 native release/tool
cases (one native Windows skip); 32 runtime cases (one native Linux skip); and
14 package-audit controls pass. Documentation structure and its 67 cases pass.
These observations do not yet supply a real numbered producer composition,
actual Windows MSI/PE and Linux DEB observations, or final native acceptance.
The initial incorrect build target, old fixture refusals and corrected checks
remain in the external request evidence rather than becoming passing claims.

A fresh one-criterion attended session `4ac6d45421394cb192ab263b9c4d895a`
asks only whether the next corrected candidate should use 0.4.1. Its state is
outside source under the request's `attended-version041-r1` directory. It is
ready and protected; no decision has been consumed. The published v0.4.0
preview is unchanged. Separate functional follow-ups remain open: the five
Windows Debug agent observations reproduced their precondition failures, and
the bounded threaded-reply recovery still did not complete. Ubuntu's original
source qualification is running. This card and BPR-1797 remain In Progress.
