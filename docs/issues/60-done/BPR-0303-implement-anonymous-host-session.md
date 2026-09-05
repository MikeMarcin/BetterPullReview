---
schema: "implement-rfc.card.v1"
id: "BPR-0303"
title: "Implement viewer-less anonymous host sessions"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0300","BPR-0302","BPR-0531"]
---

# Implement viewer-less anonymous host sessions

## Required final state

- The application host owns a stable anonymous scope without zero/default
  authenticated account identity and performs no enrollment or vault lookup.
- Anonymous/authenticated/host switches invalidate every viewer-, account-,
  capability-, queue-, epoch-, cache-, and projection-sensitive generation.
- Concurrency, cancellation, stale completion, refresh/rate refusal, shutdown,
  and repeated switching are bounded and tested with manual executors/counters.
- Mutations, triage, queue roles, and viewed/resolution actions remain
  unavailable rather than approximated. BPR-0317 owns the distinct credential-
  free public Git/cache/epoch/diff authority.

## Lane — 2026-09-02

[[BPR-0300]], [[BPR-0302]], and [[BPR-0531]] are terminal, so this card
is claimed straight into `30-in-progress/` by the integrator: its worker started in a worktree on the head that carries BPR-0531, under the transition exception. What they
leave at its door: core's `github_access_mode` and the mode-discriminated
`pull_request_authority` (zero account and viewer under public preview, by
rule); `mode_change_invalidation`, which yields the same
`invalidate_github_identity_request` an account switch yields; the
adapter's `github_provider_read_authority` chosen at provider construction;
an anonymous `fetch_pull_request` that refuses any read shape that is not
all-false; and a snapshot whose REST review comments are a distinct
unavailable-or-observed field. The host's own gates — the session read model
refusing a zero account, the capability and credential authority keys, and
the epoch-catalog key — are the surfaces this card routes an anonymous
session around, never by relaxing them.

## What this slice delivered — 2026-09-02

**A signed-out session is a typed session, never a relaxed account.**
`github_session_read_model` and `github_pull_request_snapshot` carry
`Mode`; `validate` keeps today's rule under `authenticated` and, under
`public_preview`, requires an anonymous viewer (`application::anonymous`,
every identity field empty), a nonzero profile and host, and a shape that
`reads_nothing`. Core's `activate_host_profile_intent`, `github_state`, and
`pull_request_request_context` carry the mode as trailing defaulted fields,
`session_ready` applies the two-rule account test, and every authenticated
transition is unchanged in behaviour.

**The activation path issues nothing.** `activate_public_preview` on the
host activates a GitHub.com profile signed out: no `/user` request, no
`begin_capabilities`, no capability-cache lookup or commit, and no vault
call of any kind — the counters prove all four zero. It synthesises
`CapabilityEvidence{ContractVersion 1, all-false shape}`, accepts the
`public_preview` read model, and publishes it. The anonymous scope's
identity is the profile, its revision, the host, the mode, and the identity
generation: stable across requests and distinct from every account. One
profile resolution precedes a GHES refusal, because it is the only way to
learn the server contract; that refusal then sends no viewer, capability,
read, or vault request. A null anonymous authority refuses with zero
requests of any kind.

**Reads route by mode, refusals are typed.** The host takes an optional
`github_pull_request_port* aAnonymousPullRequests`; a public-preview session
reads through it with `github_pull_request_provider_request::Mode` stamped so
the provider can refuse a mismatch, and when it is absent — today's
composition, until [[BPR-0305]] — the activation is refused as
`github_public_preview_refusal{public_preview_unavailable}` with zero
requests. Every mutation, triage, queue, viewed, and resolution intent is
refused `policy_refused` before any request, through core's legality table
(`public_preview_legality`). A rate-limited read becomes core's
`refuse_rate_limited` with limit and reset and sign-in offered, once.

**One invalidation path.** Every anonymous↔authenticated switch, in either
direction, and every host or profile switch under public preview applies
`core::mode_change_invalidation`'s request through the existing
`invalidate(…)`; the test asserts it clears exactly what an account switch
clears. Under public preview the state's account is a typed zero that the
mode discriminates, so the cache invalidation reaches
`invalidate_identity(profile, host, 0)` — harmless and identical in shape to
the account path.

**The epoch handoff is unavailable, not faked.** `epoch_catalog_authority_key`
and the epoch handoff records carry `Mode` with core's two-rule validity so
[[BPR-0317]] can key on them, but under public preview the host opens no
epoch catalog and no Git cache; `epoch_handoff_unavailable` reports the typed
state and is cleared on invalidation. Diff content under public preview is
[[BPR-0317]]'s.

Left where it was found, named for the next cards: `github_read_contract.hpp`
still builds session models and request contexts without a mode, so anything
constructing them outside the host defaults to `authenticated` — [[BPR-0305]]
must wire the real anonymous provider against that; `pull_request_request_context`
now carries the mode that `core::anonymous_request_identity` also pairs with
it, a duplication [[BPR-0307]] may collapse; no `public_preview_unavailable`
enumerator was added to `core::github_failure`, since a new enumerator would
have broken `-Werror` in the Qt adapter this slice may not edit — the typed
refusal is `application::github_public_preview_refusal`, and [[BPR-0304]]
maps it.

**`application_anonymous_read_host`.** Ten tests in
`tests/application/github_anonymous_read_host_tests.cpp` with a manual
executor and counters: zero vault, viewer, and capability traffic across an
activation and two opens; zero requests on GHES and on a null port; the
`public_preview` read model with an all-false shape and typed-unavailable
observations; stable identity across opens and distinct from an account;
two concurrent opens; cancellation mid-flight; a stale completion after a
switch rejected; the rate refusal with limit, reset, sign-in, one attempt;
shutdown mid-flight; repeated switching invalidating the account-switch set;
every mutation, triage, queue, viewed, and resolution intent refused typed.
The inventory allocates it, anchored on `application_github_host`.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The new host suite, the authenticated host, the epoch and diff hosts, core's policies, the provider and anonymous contract, the catalog storage, the manifest gate, the public-path fixtures, and both composition contracts | `ctest --preset dev -R '^(application_anonymous_read_host\|application_github_host\|application_epoch_[a-z_0-9-]*\|application_diff_host\|core_github_policy\|core_anonymous_public_read\|core_epoch_policy\|github_provider\|github_anonymous_read_contract\|sqlite_epoch_catalog_storage_contract\|test_network_isolation_manifest\|application_github_public_path_(github_com\|uiforetw_74)\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 20/20 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the nine touched files | exit 0, no output | this commit |
| The ledger's `--all` inventory form and the registered inventory gates | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all`; `ctest --preset dev -R '^(rfc0017_test_inventory\|rfc0017_test_inventory_contract)$'` | `RFC0017_TEST_INVENTORY_OK`; 2/2 | this commit |
| The inventory's own refusal table | `python3 tests/scripts/verify_rfc0017_test_inventory_tests.py` | **17 tests, OK** | this commit |
| The complete core-only matrix with the core changes | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && ctest --preset core-only --output-on-failure` | 113/113 in 286 s | this commit |
| The probe-free activation is load-bearing | The worker made the public-preview activation call `begin_capabilities`, first with the zero-account key (the lookup fails closed and the session is refused: `FAILED: the signed-out session is accepted`) and then with a synthesised nonzero viewer (`FAILED: a signed-out activation probes no capability`); restored, rebuilt, green | both variants caught | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
