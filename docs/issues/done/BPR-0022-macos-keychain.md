---
id: BPR-0022
title: Implement and verify macOS Keychain credentials
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0021]
completed: 2026-07-29
---

# Implement and verify macOS Keychain credentials

Implement the native macOS credential-vault adapter and directly prove
store/retrieve/delete of a disposable canary credential. Verify that the
secret is absent from settings, SQLite, arguments, diagnostics, logs, and UI
text and that cleanup succeeds. Exercise locked, unavailable, access-denied,
replacement, and delete-failure states; every failure must remain explicit and
must never fall back to a profile, settings, environment, or plaintext file.

> Later BPR-0029 note: this card records the historical v1 hostname-free
> service. The current adapter uses a disjoint v2 service and 574-character
> hostname-bound authority; BPR-0029 reran macOS native
> setup/lifecycle/cleanup and selected that v2 factory publicly. No v1
> migration or fallback exists.

## Result

- Added an Apple-only file-based Keychain v1 adapter that resolves the exact
  default Keychain, uses one generic-password item, makes every operation
  noninteractive, and never probes or falls back to another Keychain or
  storage implementation.
- Fixed distinct production and integration-test services and encoded the
  complete portable authority as 66 lowercase hexadecimal characters with
  explicit GitHub.com and GHES contract discriminators.
- Implemented atomic add/update/retry replacement without delete-before-write,
  one-result retrieval, exact deletion, and bounded typed status mapping.
- Kept synchronous Security framework calls on an injected host effect
  executor, separate from the serialized decision and GUI lanes, with exact
  cancellation, teardown, callback, and secret-lifetime behavior.
- Added fake-backed contract coverage for encodings, all selected status and
  failure families, duplicates, malformed results, exceptions, replacement,
  reordering, cancellation races, reentrancy, atomic concurrent teardown,
  and wiping.
- Added a resource-locked native setup/lifecycle/cleanup fixture that passed
  add, retrieve, in-place replacement, delete, absence, same-binary parent
  cleanup, and independent final cleanup for one disposable item.
- Audited runtime canaries across arguments, explicit environment, fixed
  output, executable, source, and isolated scratch/XDG files, and combined
  that evidence with source/binary rejection of settings, SQLite, UI,
  logging, plaintext, and alternate-vault paths.
- Recorded that the ad hoc linker-signed test binary proves same-binary
  cleanup, not cleanup after a crash and rebuild changes its exact code hash.
  Public application composition and stable signed-package lifecycle remain
  later work.
- Preserved local and target-native build ownership. GitHub Actions has no
  build, test, package, release, or verification role.

## Evidence

- Qt-enabled `dev` configure/build/test matrix: 28/28 passed.
- Qt-free `core-only` configure/build/test matrix: 16/16 passed.
- Focused Keychain contract/setup/lifecycle/cleanup matrix: 4/4 passed.
- Concurrent teardown regression: 25 consecutive focused runs passed.
- Independent security re-audit found no remaining issue after the atomic
  queued-work teardown fix.
- Hostile architecture-boundary tests: 40/40 passed; direct verifier passed.
- Native imports were the selected Keychain CRUD/default-Keychain surface;
  Security and CoreFoundation dependencies were directly inspected.
- The exact integration item was absent after parent and fixture cleanup; no
  matching audit scratch directory remained.
- Documentation and original source-pack validators passed.
- `git diff --check`: passed.
- GitHub Actions workflow files and tracked workflow files: zero.
