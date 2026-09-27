---
schema: "implement-rfc.card.v1"
id: "BPR-1054"
title: "Build and install a non-GUI tour runtime and bpr executable"
status: "Done"
rfcs: ["RFC-BPR-0044"]
depends_on: ["BPR-1053"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-03","milestone":"headless-manual","acceptance":["AC44-01"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC44-01","outcome":"pass","revision":"d62902a869100847dcc92d1376329c1aa9986a88","evidence":{"kind":"command","detail":"Tests-disabled tour-cli configured/built/installed; installed tour_cli_tests.py passes with no display and invalid QPA. DYLD inspection loads installed Core/Network only. Desktop app builds; application_review_tour_cold_import_public_path passes (14.07s). Core-only configure/build plus core_dependency_boundary, architecture_boundaries and application_review_tour_host pass. Local macOS install only; cross-platform distribution qualification remains later scope."}}]
---

# BPR-1054: Build and install a non-GUI tour runtime and bpr executable

Pack alias: **PACK-PRTOUR-03** (live `BPR-1054`, allocated at import).

## Outcome

Separate concrete service builds from BPR_BUILD_QT presentation gating, reuse Qt Core/Network as needed, and compose a minimal QCoreApplication headless runtime. Add CLI option/presets/install target without requiring the desktop, GUI plugins, syntax widgets, mascot, tray or updater. Preserve desktop/core-only graphs.

## Existing ownership seams

CMakeLists.txt; cmake/BprDependencies.cmake; src/adapters/github/CMakeLists.txt; src/app/CMakeLists.txt; existing platform vault/process paths.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Tests-disabled headless executable runs without DISPLAY/WAYLAND_DISPLAY or GUI initialization; binary/dependency inspection proves no GUI/Widgets/QPA load. Focused desktop/core-only regressions remain passing on available targets.

Acceptance ownership: AC44-01

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-03 was allocated as live `BPR-1054` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
