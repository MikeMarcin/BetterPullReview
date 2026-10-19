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

## Engineering evidence and preproduction boundary

The source-input declaration now carries six pinned build files: the original
provider, two compatibility headers, standalone CMake/README and unchanged
hardening helper. The standalone entry point resolves its helper inside the
asset and uses an explicit extracted source path; it includes no acquisition
provider or application tree. Installed source offers and the guide identify
that entry point and permit modified recipient source. This is technical recipe
work, with no UI change or new core infrastructure.

The actual Mac r2 assessment finishes in 5.662 seconds. From the verified
archive and freshly generated xdiff-component source asset, it builds both
arm64 and x86_64 slices, verifies the exact seven C11 compile commands with
canonical hardening options, and compares all 30 defined arm64 exports with
the preserved signed DFB library. A separately linked C consumer exercises
xdl_diff with Myers/indent and exact expected output. Editing recipient xutils.c
adds a callable marker, preserves every original export and repeats the correct
API result after rebuild/relink. The corrupt-archive writer, missing helper,
header/source and incorrect-result controls all refuse. Temporary source and
build roots are removed; the delivered library hash is unchanged.

The receipt is `.mask/sessions/release-v0.3.0/xdiff-recipient-r2/receipt.json`.
Its sourceRevision is the then-current claimed HEAD; recipe/declaration bytes
were under assessment before integration, not a new release asset. Input hashes
and the final commit binding accompany the coordinator handoff. This is an
actual universal build and arm64 execution/export comparison, with no native
Intel desktop or Windows/Linux recipe-execution claim. Windows import-library
and API controls remain external native verification work.

Focused package-runtime verification passes 24/25 with its configured Linux
baseline skip on Mac; separate source-asset verification passes all21. The
coordinator owns final all-platform source/SPDX/notices/package reproduction
and complete public audits. The card stays In Progress for that acceptance;
all old assets and broader test/native/vault/privacy/signing/legal/security
obligations remain. No claim or receipt from the component-only assessment
repairs the old asset or certifies the application replacement course.
