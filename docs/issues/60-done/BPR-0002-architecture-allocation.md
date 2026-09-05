---
schema: "implement-rfc.card.v1"
id: "BPR-0002"
title: "Establish RFC-BPR-0001 architecture allocation and repository foundation"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: ["BPR-0001"]
completed: "2026-07-29"
---

# Establish RFC-BPR-0001 architecture allocation and repository foundation

Create the C++23/CMake target graph, module roots, typed architectural
vocabulary, warnings/hardening defaults, test roots, fixture roots, and
dependency-boundary checks. Prove that core, host, Qt adapter, external
adapters, and composition ownership match the RFC.

Completion requires focused configure/build checks and an updated architecture
ledger in `docs/verification/RFC-BPR-0001.md`.

## Result

- Added a C++23 CMake project with Qt-enabled and Qt-free presets.
- Established separate core, application, GitHub/Git/SQLite/AI adapter, Qt
  adapter, and composition targets with inward dependencies.
- Added first-party warnings and platform hardening defaults.
- Added typed core identities, authority categories, intents, observations,
  requests, events, deltas, diagnostics, and application state vocabulary.
- Added a narrow host request port and an automated core dependency scan.
- Established the packaging root and ignored out-of-source build trees.

## Evidence

```text
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Both configurations built `better_pull_review_core`, and
`core_dependency_boundary` passed.
