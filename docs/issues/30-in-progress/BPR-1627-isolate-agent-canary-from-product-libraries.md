---
schema: "implement-rfc.card.v1"
id: "BPR-1627"
title: "Keep the agent canary linked only to its actual dependencies"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"ead4e081d323b6e4864767ff2e68a147f0a5e03c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1627: Keep the agent canary linked only to its actual dependencies

The frozen macOS normal gate fails `qt_agent_launch_canary` at its exact child-environment assertion. The standard-library-only canary inherits `bpr_test_product_diff` through the test directory, bringing unrelated application/CoreFoundation and xdiff dependencies. The frozen image reports its two supplied synthetic names plus CoreFoundation's startup `__CF_USER_TEXT_ENCODING`. The coordinator authorized removing those inherited product links from this fixture, preserving its explicit Windows probe libraries and exact two-name assertion.

## Acceptance

- AC1: Bind the observed extra environment name and unrelated loader dependencies to the frozen canary and remove only this fixture target's inherited product-library links, preserving explicit Windows `winmm` and `ntdll`.
- AC2: Compile the unchanged canary source with its actual standard/platform dependencies in a disposable native build, verify its exact two-name synthetic environment and protocol success, and show that an additional synthetic name still violates that exact set. Preserve the existing profile, assertion and production launcher/environment code.
- AC3: Self-review the bounded target diff, pass documentation and diff checks, commit explicit paths, and hand matching native CMake/CTest integration reruns to the coordinator without claiming them passed.

The coordinator retains all full/native builds and release producer slots. No signing, credential, publication, dependency adoption or runtime privacy-policy change is delegated. The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` retain focused evidence and pending release obligations.
