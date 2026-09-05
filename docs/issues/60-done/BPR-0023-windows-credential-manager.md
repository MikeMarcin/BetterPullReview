---
schema: "implement-rfc.card.v1"
id: "BPR-0023"
title: "Implement Windows Credential Manager credentials"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0021"]
---

# Implement Windows Credential Manager credentials

Implement the native Windows Credential Manager adapter and prepare the
target-native fixture that will prove store/retrieve/delete of disposable
canary credentials. Exercise unavailable storage, access denial, replacement,
and delete failure through the injected contract; every failure must remain
explicit and must never fall back to settings, environment, or a plaintext
file.

GitHub Actions is not an acceptable evidence surface.

> Later BPR-0029 note: this card records the historical v1 target in its
> implementation/cross-PE evidence. Current source uses a disjoint v2 prefix
> and 574-character hostname-bound authority. BPR-0036 retains target-native
> current-format lifecycle, cleanup, and public-fixture evidence; no v1
> migration or fallback exists.

## Result

- Added a WIN32-only wide-character Credential Manager adapter using one
  generic, local-machine-persistent credential per exact portable authority.
- Fixed disjoint production and integration-test target prefixes and the
  66-character GitHub.com/GHES authority encoding.
- Implemented single-call create-or-replace, exact read/delete, bounded
  native ownership and wiping, conservative typed status mapping, effect-lane
  execution, cancellation, and atomic queued-work teardown without fallback.
- Added fake-backed contract coverage, hostile architecture boundaries, and a
  resource-locked native setup/lifecycle/cleanup fixture.
- Passed the macOS `dev` and `core-only` matrices, 58/58 hostile architecture
  tests, documentation and diff gates, and local x64 MinGW compile/link plus
  PE import inspection. The complete fake-backed Windows contract executable
  also passed under AddressSanitizer and UndefinedBehaviorSanitizer with
  aborting native API shims, and the production source passed GCC static
  analysis. These results prove isolation and preparatory portability only.
- Kept the Windows runtime claim `Planned`. By product-owner direction, the
  real Windows x64 configure, build, test, lifecycle, cleanup, and runtime
  dependency checks are tracked separately by BPR-0036 so they do not block
  the remaining implementation slices.

## Evidence

- `cmake --preset dev`, `cmake --build --preset dev`, and
  `ctest --preset dev --output-on-failure`
- `cmake --preset core-only`, `cmake --build --preset core-only`, and
  `ctest --preset core-only --output-on-failure`
- hostile architecture suite: 58/58
- x64 MinGW strict compile/link and PE import inspection
- fake-backed contract executable under AddressSanitizer and
  UndefinedBehaviorSanitizer with aborting native API shims
- GCC static analysis of the production adapter
- documentation, source-pack, workflow-absence, and `git diff --check` gates
- BPR-0036 retains every target-native Windows claim and cleanup obligation

Completed: 2026-07-30
