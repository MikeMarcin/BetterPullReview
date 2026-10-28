---
schema: "implement-rfc.card.v1"
id: "BPR-1797"
title: "Prepare, qualify and publish v0.4.0"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v040-owner-request-r1-20261028", "baseline": "7dc90f2d8e7a6ef9c852a1db93f6c20e2c1570ae", "execution_strategy": "coordinator-implementer", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.0-20261028", "milestone_plan": {"id": "release-v040", "path": "docs/rfcs/plans/release-v040/milestone-plan.json"}}
---

# Prepare, qualify and publish v0.4.0

The owner requested `$publish-release v0.4.0` with an implement-rfc coordinator and one implementer for needed fixes. The owner also amended acceptance: “Anything that comes up for this release that was previously waived is waived again, carry it forward.” The request authorizes the existing full macOS universal, Windows x64 and Ubuntu 26.04 WSL x64 release route, signing and publication.

## Acceptance

- AC1: Prepare version 0.4.0 and grounded user-facing notes; freeze one clean common committed candidate while preserving unrelated work.
- AC2: Reconcile existing native jobs, pass the all-platform Release/Debug/distribution compilation barrier, and run comprehensive native Release plus Debug functional selection. Preserve actual failures/skips, fix new blockers in explicit slices, and record carried matching historical waivers separately.
- AC3: Produce and audit the complete signed binaries, exact corresponding source, SPDX, checksums and provenance through the existing native tools. Run fresh source/security and native trust checks; retain known M8/M9, Intel, allocator/advisory/fuzz and other matching historical limitations under the owner amendment.
- AC4: Preflight and publish the qualified authorized candidate through the existing publisher. Verify remote state, all uploaded hashes and native trust after download; complete tracker/docs and durable command/evidence upkeep.

## Initial reconciliation

Entry source is clean `7dc90f2d8`; MASK registration changes no tracked declarations. Broad changes since v0.3.4 require comprehensive qualification. The standing Debug policy omits exactly the eight named dedicated performance courses and retains Debug functional checks. Old observations and failures keep their original source identity. Mac capacity is initially 5 GiB, below the roughly 11 GiB graph footprint before temporary/nested/package work. Native producers and signer state are under inspection. No release/tag mutation has occurred.

## Prepublication UTF-8 correction

The Windows focused course passed five checks and failed both tour publication
progress-stage checks. A native MSVC/Qt probe confirmed that an ordinary narrow
smart-quote literal becomes code-page byte 93, which Qt decodes as replacement
efbfbd; explicit UTF-8 bytes decode correctly as e2809c. The application
stage formatter and its owning expectations now name explicit UTF-8 bytes.
No compiler flags change. Focused native verification and fresh package/source
binding remain required before publishing the corrected candidate. The signed
previous candidate is retained externally and has never been published.

The owner subsequently requested Mac-first prerelease, then Windows assets on
the same prerelease, with comprehensive qualification afterward. Stable
promotion and Ubuntu artifacts retain their complete gates. The exact external
amendment and new HarfBuzz decision packet are in the recorded runtime root;
no passing qualification or security decision is inferred.
