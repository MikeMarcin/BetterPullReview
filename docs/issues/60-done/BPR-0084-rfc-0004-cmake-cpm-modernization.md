---
schema: "implement-rfc.card.v1"
id: "BPR-0084"
title: "Modernize CMake, CPM dependencies, and SQLite acquisition"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0075"]
---

# Modernize CMake, CPM dependencies, and SQLite acquisition

Raise the build baseline to CMake 4.4.2, replace test selection with standard
`BUILD_TESTING`, centralize target-scoped dependency ownership, and upgrade the
developer, core-only, and Windows presets to the current schema and workflow
model.

Use installed package providers for prebuilt dependencies: Qt uses its CMake
config package, Python and Git use their standard CMake find modules, and Linux
libsecret uses `pkg-config`. Require their conventional imported targets before
product subdirectories are evaluated. Qt is a package-only dependency and must
never be downloaded automatically.

Use pinned CPM.cmake 0.43.1 for source-managed dependencies. Replace the tracked
SQLite amalgamation with the exact upstream SQLite 3.53.4 archive, acquired
download-only through CPM and verified by the recorded archive SHA3-256,
source-file digests, version, and source identity. Preserve the static
`BetterPullReview::SQLite3` target and every existing compile/security
definition, with no system SQLite fallback.

## Required direct evidence

- Dependency contracts prove discovery is centralized, version floors and
  expected imported targets are required, Qt cannot be fetched, and test- or
  Qt-only packages are omitted from graphs that do not need them.
- SQLite cold fetch, warm-cache reuse, explicit offline reuse/failure, archive
  and source-identity mismatch, target compile policy, and system-fallback
  absence are directly covered.
- Clean developer, core-only, and tests-disabled Release matrices pass under
  CMake 4.4.2 on macOS. Ubuntu execution is deferred and not claimed. Windows
  preset structure is covered without claiming unperformed native Windows
  execution, which remains in the existing real-Windows evidence cards.
- The architecture/dependency scans, source-dump contracts, documentation
  validator, binary/runtime audits, and `git diff --check` pass.

## Documentation and closeout

Update the public build prerequisites and commands, build/package dependency
records, SQLite provenance and license instructions, RFC-BPR-0004 verification
ledger, issue board, and any agent-owned build-selection contracts affected by
the migration. Run a conservative simplification pass and an independent
capability-critical review before moving this card to `done/`.

## Result

- The direct 20-case dependency-modernization contract passes under CMake
  4.4.2. It covers the baseline and presets, obsolete-option rejection,
  centralized package ownership, provider auditing, conditional discovery,
  package-only Qt, CPM and SQLite pins, online/cache/offline behavior,
  provider-path tamper and source-identity rejection,
  target/config/redirect/dependency-provider injection refusal, caller module
  shadowing (including transitive `FetchContent`), controlled SQLite staging
  under a poisoned-cache-header fixture, static compile-definition parity, and
  absence of vendored or system SQLite fallback.
- The 104-case architecture-boundary contract passes after changing the build
  ownership checks to require the centralized imported-target provider rather
  than discovery calls in product subdirectories.
- The clean macOS `dev` matrix passed 74/74, the hardened `core-only` matrix
  passed 33/33, and the tests-disabled Release graph passed its self-test,
  binary, and runtime audits. Final focused contracts, documentation
  validation, and `git diff --check` pass.
- The conservative simplification pass completed. Capability-critical review
  found cache-sibling poisoning and dependency-provider/module-shadowing
  bypasses; controlled staging, absolute module ownership, top-level provider
  refusal, and direct adversarial tests resolve those findings.
- Ubuntu execution was skipped at the product owner's direction and no current
  Linux run is claimed. Native Windows execution is likewise not claimed;
  Windows preset coverage remains structural.

The checked-in build and developer documentation describe caller-installed
packages for prebuilt dependencies and verified CPM source acquisition for
SQLite. `BPR_DEPENDENCY_OFFLINE=ON` disables CPM network acquisition while
leaving local installed-package discovery available.
