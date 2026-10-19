---
schema: "implement-rfc.card.v1"
id: "BPR-1656"
title: "Ship a standalone xdiff recipient rebuild recipe"
status: "In Progress"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0082"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"dfb5f95f3a6fc02ca58148d1d99b7ba7cf4d4bbd","reservation":"docs/issues/_reservations/BPR-1656--BPR-1656--025e2086fd06.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["packaging/dependencies.json","packaging/package_runtime.py","third_party/git-xdiff/rebuild/CMakeLists.txt","third_party/git-xdiff/rebuild/README.md","third_party/git-xdiff/PROVENANCE.md","tests/scripts/package_runtime_tests.py","tests/scripts/xdiff_recipient_rebuild_tests.py","docs/development/release-packages.md"],"boundary":"Coordinator explicitly reopens the freeze for the standalone corresponding-source recipe; no production C/C++/provider/hardening change, producer mutation or runtime qualification"}
---

# BPR-1656: Ship a standalone xdiff recipient rebuild recipe

Both preserved DFB source ZIPs contain the original xdiff acquisition provider
and two compatibility headers. The provider includes absent AitoCpm/Hardening
helpers, requires CPM and application-root paths, and supplies no standalone
entry point. Exact mechanical inventory checks pass their declared three files,
but do not prove recipients can rebuild the separately replaceable LGPL library.
The coordinator explicitly invalidates the source freeze for this bounded repair.

## Acceptance

- AC1: Retain original final assets and the actual source-material gap. Ship pinned original archive, compatibility headers, standalone recipient CMake project/instructions and every local helper it needs, without a BetterPullReview checkout, CPM, shared cache or network acquisition. Preserve the original seven C11 shared-library sources, output/export and hardening settings; original archive pins must not prohibit modifying recipient-extracted source.
- AC2: From only the generated component source asset, configure/build in fresh disposable roots, compare defined exports against the exact delivered library and link/run a consumer through the public xdl_diff C interface. Modify recipient source, rebuild/relink and prove the modification executes while the original API remains compatible. Corrupt original archive, missing helper/header/source and incorrect expected results must refuse. Record actual platform/architecture and retain independent changed-surface review; no native-desktop or other-platform execution claim follows.
- AC3: Reproduce affected final assets at the next coordinated common source, including matching separate source ZIP, installed notices/offers/provenance/SPDX and actual complete package/security audits. Preserve DFB assets unchanged and every existing native/signing/legal/security/test obligation. This remains pending external producer acceptance at the engineering freeze.
- AC4: Complete focused source/package tests, canonical criterion binding, documentation and intentional allocated-card/plan/ledger upkeep before the source freeze. Keep later operational acceptance receipts outside source until the coordinator's postrelease administrative boundary.
