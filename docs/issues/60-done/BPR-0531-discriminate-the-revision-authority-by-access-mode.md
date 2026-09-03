---
id: BPR-0531
title: Discriminate the revision authority by access mode in core
status: Done
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

## What this slice delivered — 2026-09-02

**The authority says which authority it is.** `core::pull_request_authority`
gains a trailing `github_access_mode Mode`, value-initialised, inside the
defaulted three-way comparison. `epoch_model.hpp` declares the enumeration
opaquely (`enum class github_access_mode : std::uint8_t;`) and never
includes the anonymous policy header; `epoch_transition.cpp` and
`diff_model.cpp` include it where they name enumerators, and a
`static_assert` pins that the value-initialised mode is `authenticated`, so
every one of the twenty-one existing designated initialisers means exactly
what it meant.

**Two validity rules, one per mode.** `valid()` refuses an out-of-range mode
outright; under `authenticated` it requires a nonzero account and viewer
exactly as before; under `public_preview` it requires both to be zero — an
anonymous read cannot wear an account's identity — and validates every other
field identically. Two authorities identical except for mode compare unequal
and order deterministically, and the epoch transition that compares the
current authority against the prior one treats a mode change as an
authority change.

**Persisted anchors keep matching.** `append_authority` encodes the eleven
original fields as before and appends the mode byte only when the mode is
not `authenticated`. An authenticated authority therefore encodes byte for
byte as it did before this card, so every `file_comparison_digest` the
review ledger and the recovery store persisted still matches, and the five
known-answer vectors in `core_diff_model` pass unmodified — that is the
compatibility proof. A public-preview authority encodes one byte longer than
any authenticated one, so the discrimination rests on the message, not on
the identity fields. The worker's first version appended the byte
unconditionally and had repinned all five vectors; that was reversed before
integration because a re-keyed digest would have orphaned every stored
anchor on upgrade. Making the byte unconditional belongs with a domain-string
bump and a migration, which is [[BPR-0317]]'s.

**Left authenticated-only, for [[BPR-0303]] and [[BPR-0317]] to decide.**
Four core sites compare a viewer review's actor against `Authority.Viewer` —
`plan_epoch_preparation`, `plan_epoch_mutation`, `resolve_comparison`, and
the revision-accepted handler. Under public preview the viewer is zero, so a
revision carrying a viewer review is refused as `invalid_observation` or
`invalid_revision`: fail-closed and consistent with
`observes_viewer_state(public_preview) == false`, but with a generic reason
code, and the `current_viewer_review` comparison anchor can never resolve in
public preview. Unchanged here.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| Core, the SQLite and Git adapters that persist or key on the authority, and the application's epoch, diff, and host courses | `ctest --preset dev -R '^(core_epoch_policy\|core_anonymous_public_read\|core_github_policy\|core_diff[a-z_]*\|core_epoch[a-z_]*\|sqlite_epoch_catalog_[a-z_]*\|sqlite_review_ledger_storage\|sqlite_review_mutation_recovery_storage\|git_cache_contract\|application_epoch_[a-z_0-9-]*\|application_diff_[a-z_]*\|application_github_host\|application_review_round_trip_composition_contract)$' --output-on-failure` | 23/23 passed | this commit |
| The architecture boundaries hold with the opaque declaration | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on the five touched files | exit 0, no output | this commit |
| The complete core-only matrix | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && ctest --preset core-only --output-on-failure` | 112/112 in 284 s | this commit |
| The public-preview zero-identity rule is load-bearing | The worker dropped `Account == 0` from the public-preview branch, rebuilt, ran `core_epoch_policy`; restored from a byte copy | exit 8, `FAILED: a public-preview authority carrying an account is refused: an anonymous read cannot wear an account's identity`; restored, 7/7 | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
