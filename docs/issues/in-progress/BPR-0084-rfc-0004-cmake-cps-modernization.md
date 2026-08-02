---
id: BPR-0084
title: Modernize CMake, CPS dependencies, and SQLite acquisition
status: In Progress
rfc: RFC-BPR-0004
depends_on: [BPR-0075]
---

# Modernize CMake, CPS dependencies, and SQLite acquisition

Raise the build baseline to CMake 4.4.2, replace test selection with standard
`BUILD_TESTING`, centralize target-scoped dependency ownership, and upgrade the
developer/core-only/Windows presets to the current schema and workflow model.

Prebuilt Qt, libsecret, Python, and Git artifacts must enter the product graph
through validated machine-local CPS descriptions. Producer CMake or pkg-config
metadata may run only in an isolated bootstrap configure; the main graph must
search the generated CPS prefix exclusively and fail closed on missing, stale,
below-floor, non-CPS, or untranslatable package metadata.

Replace the tracked SQLite amalgamation with the exact upstream SQLite 3.53.4
archive acquired download-only through pinned CPM.cmake 0.43.1 and verified by
the recorded SHA3-256, version, and source identity. Preserve the static
`BetterPullReview::SQLite3` target and every existing compile/security
definition, with no system SQLite fallback.

## Required direct evidence

- Configure-time and fixture contracts prove the main graph resolves Qt,
  libsecret, Python, and Git only from `.cps` files and rejects CMake-script,
  pkg-config, stale, below-floor, malformed, or unsupported metadata.
- SQLite cold fetch, warm-cache reuse, explicit offline reuse/failure, archive
  and source-identity mismatch, target compile policy, and system-fallback
  absence are directly covered.
- Clean developer, core-only, and tests-disabled Release matrices pass under
  CMake 4.4.2; target-native macOS and Linux evidence is recorded. Windows
  preset/CPS structure is covered without claiming unperformed native Windows
  execution, which remains in the existing real-Windows evidence cards.
- The architecture/dependency scans, source-dump contracts, documentation
  validator, binary/runtime audits, and `git diff --check` pass.

## Documentation and closeout

Update the public build prerequisites and commands, build/package dependency
records, SQLite provenance and license instructions, RFC-BPR-0004 verification
ledger, issue board, and any agent-owned build-selection contracts affected by
the migration. Run a conservative simplification pass and an independent
capability-critical review before moving this card to `done/`.

## Current evidence

- Native macOS arm64 uses CMake 4.4.2, AppleClang 16, Ninja 1.13.2,
  Qt 6.11.1, Python 3.14.6, and Git 2.39.5. A clean `dev` configure built all
  143 targets and passed 74/74 tests in 242.11 seconds. Its nested
  tests-disabled Release composition also built, ran both production
  self-tests, and passed fixture-seam, dependency, and runtime audits.
- The fresh `core-only` configure generated only the Python CPS adapter: Qt,
  libsecret, and Git producer discovery were all disabled. Its 69-target build
  and 33/33 test matrix passed in 40.04 seconds.
- The 14-case dependency modernization contract covers single- and
  multi-config CPS generation, paths with spaces, configurations, transitive
  requirements, known feature translation, version floors, stable refresh,
  stale/tampered documents and artifacts, injected CMake package scripts,
  unsupported/incomplete producer metadata, CPM pinning, SQLite archive/source
  identities, warm offline reuse, uncached offline failure before network
  access, static linkage, compile-definition parity, and system-fallback
  absence.
- CMake 4.4.2 cannot import CPS `executable` components. The generated Python
  and Git packages therefore carry exactly one fingerprinted executable path
  in an interface metadata component; the strict resolver validates it before
  materializing `Python3::Interpreter` or `Git::Git`. No Find-module variable
  or producer CMake script enters the product graph.

The target-native Ubuntu 26.04 matrix, conservative simplification pass, and
capability-critical review remain open before closeout. Native Windows
execution is not claimed by this card.
