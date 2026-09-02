---
id: BPR-0531
title: Discriminate the revision authority by access mode in core
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0300]
---

# Discriminate the revision authority by access mode in core

## Why

The RFC-BPR-0017 plan's Core row owns "discriminated authenticated-account
versus anonymous-public revision authority". [[BPR-0300]] delivered the
signed-out mode, the intent policy, and request identity, but
`core::pull_request_authority` in `src/core/include/bpr/core/epoch_model.hpp`
still carries an account and a viewer and is valid only when both are
nonzero. Every downstream key reuses that one shape: the application's
epoch-catalog authority key and epoch handoff records, the Git cache open
request and the on-disk cache manifest, and the SQLite `repository_scope`
schema with its nonzero `account_id` check. Found by the [[BPR-0303]] and
[[BPR-0317]] scouts on 2026-09-02: neither card can build a viewer-less
scope or an authority-scoped cache on a type that has no way to say which
authority it is, and the plan forbids both a zero-account sentinel and
cross-authority namespace reuse. This card closes the gap in core so those
two cards key off a discriminant rather than inventing one each.

## Required final state

- `core::pull_request_authority` carries the access mode as a typed
  discriminant. Under `authenticated` the account and viewer must be nonzero
  exactly as today; under `public_preview` there is no account and no
  viewer to carry, so both are absent by rule and a nonzero value is a
  validity failure — an anonymous authority cannot alias an account's.
- Two authorities that differ only in mode never compare equal, and every
  place core derives a key, ordering, or hash from the authority includes the
  mode.
- Every existing authenticated caller compiles and behaves unchanged; the
  discriminant defaults to `authenticated`.
- Core tests cover both validity rules, the aliasing refusal, inequality
  across modes with otherwise identical fields, and the unchanged
  authenticated behaviour.
- No application, adapter, or composition change: the application keys
  ([[BPR-0303]]) and the Git cache manifest and SQLite schema ([[BPR-0317]])
  adopt the discriminant in their own slices.

## Verification

```text
ctest --preset dev -R '^(core_anonymous_public_read|core_epoch[a-z_]*|core_github_policy)$' --output-on-failure
cmake --build build/dev --target check_boundaries
ctest --preset core-only --output-on-failure
```

## Lane — 2026-09-02

Claimed directly into `30-in-progress/` by the integrator on creation, under
the transition exception; its one dependency is terminal. [[BPR-0303]] and
[[BPR-0317]] now depend on it.
