---
id: "RFC-BPR-0001"
title: "Product Contract, Native Architecture, and Repository Foundation"
status: "Auto-Verified"
area: "BPR"
milestone: "M1 Native Read-Only Workbench"
depends_on: []
pack_version: 2
---

# RFC-BPR-0001: Product Contract, Native Architecture, and Repository Foundation

## Implementation status

Auto-Verified on 2026-07-29 against the directly exercised macOS development
host. The clean Qt-enabled matrix passed 14/14, the Qt-free matrix passed 9/9,
the real application public path and rendered inspection passed, dependency
and live-process audits passed, documentation and source-pack validation
passed, and the repository selected no GitHub Actions workflow. Clean
Windows/macOS/Linux delivered-package certification remains an explicit
RFC-BPR-0014 requirement.

## Summary

Establish BetterPullReview as a native, local, cross-platform GitHub code-review client with GitHub as the sole collaborative source of truth, built as a deterministic functional core inside an imperative shell composed of an application host and a Qt presentation adapter.


## Motivation

BetterPullReview must be acceptable on a corporate workstation while remaining high-performance and portable. The foundation therefore needs a small native attack surface, explicit module boundaries, and a product contract that prevents the local workbench from becoming a competing review system.

## Goals

- Establish C++23 and Qt 6 Widgets as the primary implementation stack.
- Build the application around a deterministic standard-C++ functional core, imperative application host, and Qt presentation adapter.
- Establish typed intents, events, bounded observations, requests, deltas, diagnostics, and one serialized application decision lane.
- Establish a platform-neutral native application shell and documented local
  build entry points; direct supported-platform certification belongs to
  RFC-BPR-0014.
- Make the GitHub pull request authoritative for all shared review state.
- Prohibit embedded web runtimes, hosted BetterPullReview services, and repository code execution.
- Establish documentation, test, fixture, and packaging roots suitable for subsequent RFCs.

## Non-goals

- Fetching a real pull request.
- Implementing the final diff renderer or review workflow.
- Building a browser extension, local service, plugin system, or auto-updater.
- Building, testing, packaging, or releasing BetterPullReview through GitHub
  Actions.

## Build ownership

BetterPullReview builds are initiated by developers or release operators using
the checked-in CMake configuration. GitHub Actions is not a build, test,
package, or release dependency. This foundation RFC proves the architecture
and current-host native public path. RFC-BPR-0014 owns direct clean-environment
Windows, macOS, and Linux package installation, launch, runtime, and uninstall
evidence.

## Current core policy — 2026-10-18

The [adopted external-determinism amendment](amendments/RFC-BPR-0001-external-determinism-r1.md)
supersedes library-specific purity-exception rationale. Dependency approval,
implementation constraints and original acceptance obligations remain.

## Architectural allocation

This RFC establishes the architecture for every dependent RFC. The complete normative source is [Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter](../architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md), specialized by [BetterPullReview System Architecture](../architecture/system-architecture.md).

### Deterministic functional core

- Own compact interaction state, semantic intents/events, command legality, validation, transition functions, request identity/generation, semantic deltas, and reason-coded diagnostics.
- Apply the [external-determinism contract](../architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism) to computation, with explicit semantic inputs/state and host-owned effects and lifecycle. Keep the core Qt-free; dependency approval and private implementation ownership remain separate constraints.
- Permit deterministic in-place transitions, retained output buffers, scratch storage, and feature-composed transition functions rather than requiring immutable full-state copies or one giant reducer.

### Application host

- Own the composition of GitHub, Git, SQLite, AI, filesystem, credential, clock, worker, cache, persistence, diagnostics, cancellation, and lifecycle capabilities.
- Supply bounded immutable observations to the core, execute typed semantic requests, and return completions as typed events.
- Apply application decisions on one serialized lane and reject/suppress stale work using explicit session, revision, and generation identity.

### Qt presentation adapter

- Own `QApplication`, windows, widgets, `QAction`, Qt models, delegates, focus, layout, accessibility, themes, native presentation, and event translation.
- Translate Qt input into semantic intents and apply projections/deltas incrementally.
- Never own canonical GitHub/ledger state, semantic command policy, asynchronous orchestration, or provider access.

### Composition and performance

The composition root wires the concrete host, core state, and Qt adapter. Routine behavior must scale with the active set plus the changed set. The first foundation slice must demonstrate the complete event path from Qt event to core transition to host request/completion to incremental Qt projection.

## Decisions

1. The application uses Qt Widgets, not Qt WebEngine or QML, for the primary product.
2. The functional core has no Qt or effectful/provider dependency and owns semantic decisions, compact interaction state, validation, request identity, deltas, and diagnostics.
3. The application host owns I/O, services, authority sessions, jobs, caches, persistence, cancellation, lifecycle, and request execution.
4. The Qt presentation adapter is part of the imperative shell; it owns Qt objects, event translation, actions, models, focus, layout, accessibility, themes, and incremental projection application.
5. SQLite, Git, GitHub, and AI are host capabilities/adapters outside the functional core.
6. The core decides, the host performs, and Qt presents.
7. Shared GitHub actions and private local state are modeled separately from the first schema.
8. All external content is treated as untrusted even before later parser hardening.
9. Routine work scales with the active set plus the changed set; models and projections update incrementally.
10. The architecture is implemented as constraints and focused feature cores, not a generic framework or giant reducer.

## Required repository foundation

- Portable CMake presets and conditional platform toolchain entry points
  suitable for later clean-environment verification.
- First-party warning and hardening defaults.
- Unit, contract, integration, UI, fixture, benchmark, and fuzz test roots.
- Architecture and security documents owned by explicit repository scopes.
- A native empty-state application with diagnostics showing build version, platform, and selected configuration without secrets.
- Separate build targets for the functional core, application host/ports, external adapters, Qt presentation adapter, and composition root.
- A deterministic manual executor and fake-port harness for host tests.
- Dependency-boundary checks that fail when Qt/provider headers enter the core or widgets call effectful adapters.

## Acceptance criteria

- The standard-C++ functional-core library builds without Qt include or link dependencies.
- The Qt application launches and renders a native empty-state workbench on
  the directly exercised development host, and the CMake configuration exposes
  portable Ninja and conditional Windows MSVC developer entry points without
  hosted build infrastructure.
- A dependency scan proves no WebEngine, Chromium, Node, Electron, Tauri, or local HTTP server dependency is selected.
- The product-contract tests distinguish GitHub shared state from local private/derived state.
- Repository instructions and agent documentation identify module ownership, dependency direction, threading, security, build, test, and package commands.
- The application has no listening port, telemetry, remote content loading, plugin discovery, or automatic update behavior.
- A complete fixture workflow proves Qt event -> semantic intent -> bounded observations -> deterministic transition -> host request/completion -> incremental Qt projection, with no direct widget effect call.
- A deterministic replay produces identical core state, requests, deltas, and diagnostics, and a manual executor proves stale completion rejection.
- Target/dependency checks prove the host and Qt adapter depend inward while the core remains standalone.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Native shell is selected | Real application on the current host | Launch and render the public path; inspect selected Qt modules |
| Functional core is Qt-free | Link and include graph | Build domain target in a toolchain with Qt unavailable |
| No browser/listener | Dependency and runtime scan | Inspect linked libraries and open sockets while running |
| GitHub is shared authority | Domain contract tests | Simulate local disagreement and confirm shared state resolves to GitHub snapshot |
| Architecture flow is selected | Core/host/Qt integration harness | Replay a full intent/request/completion/delta workflow and inspect dependency graph |
| Platform configuration is explicit | CMake presets and target graph | Validate portable Ninja and Windows MSVC presets; defer clean native package execution to RFC-BPR-0014 |

## Documentation impact

Product contract, repository map, architecture, threading, security posture, build instructions, test strategy, and packaging baseline are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
