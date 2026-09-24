---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-03"
title: "Build and install a non-GUI tour runtime and bpr executable"
status: "Backlog"
rfcs: ["RFC-BPR-0044"]
depends_on: ["PACK-PRTOUR-02"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "headless-manual", "acceptance": ["AC44-01"]}
---

# Build and install a non-GUI tour runtime and bpr executable

## Outcome

Separate concrete service builds from BPR_BUILD_QT presentation gating, reuse Qt Core/Network as needed, and compose a minimal QCoreApplication headless runtime. Add CLI option/presets/install target without requiring the desktop, GUI plugins, syntax widgets, mascot, tray or updater. Preserve desktop/core-only graphs.

## Existing ownership seams

CMakeLists.txt; cmake/BprDependencies.cmake; src/adapters/github/CMakeLists.txt; src/app/CMakeLists.txt; existing platform vault/process paths.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Tests-disabled headless executable runs without DISPLAY/WAYLAND_DISPLAY or GUI initialization; binary/dependency inspection proves no GUI/Widgets/QPA load. Focused desktop/core-only regressions remain passing on available targets.

Acceptance ownership: AC44-01

## Execution boundary

Milestone: **headless-manual**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
