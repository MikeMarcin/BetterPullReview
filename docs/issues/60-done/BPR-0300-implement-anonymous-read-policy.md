---
schema: "implement-rfc.card.v1"
id: "BPR-0300"
title: "Implement deterministic anonymous-read policy"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0035","BPR-0312"]
---

# Implement deterministic anonymous-read policy

## Required final state

- Core owns explicit public-preview mode, stable request/revision identity,
  legal read-only intents, unavailable viewer/thread-resolution/viewed/action
  observations, rate/private/GHES reasons, and authenticated-switch policy.
- No provider, URL, authorization, vault, account credential, clock, thread,
  filesystem, Qt, database, or default synthetic viewer enters core.
- Replay/property tests cover every legal/illegal action, stale/duplicate
  observation, switch, refusal, rate terminal, and missing capability.

## Lane — 2026-09-02

[[BPR-0312]] is satisfied: the owner's verdict validated on 2026-09-02 and
the four public-preview definitions are bound in
`docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json`.
Two of them shape this card's core. The mode is named "Public preview" and
is stated as signed out wherever it is named, never as the authenticated
workbench with fields missing, so core's mode is explicit and its
unavailable observations are typed rather than defaulted. Budget exhaustion
is a typed refusal that names the limit and the reset time, and every
refusal is a place the sign-in handoff is offered from, so the rate
terminal and the authenticated-switch policy must carry what a presentation
needs to say that. [[BPR-0035]] is terminal. Both dependencies hold, so this
card moves to `20-ready/` as generic work runnable on any station.

## What this slice delivered — 2026-09-02

**Public preview is a mode of its own.** `bpr::core::github_access_mode`
has two enumerators, `authenticated` and `public_preview`, and nothing in
core infers the second from a zero account: `test_the_mode_and_not_the_account_decides`
proves a populated account changes no answer under public preview and an
empty one changes none under authentication. The new pair
`src/core/include/bpr/core/anonymous_read_policy.hpp` and
`src/core/src/anonymous_read_policy.cpp` sits beside `github_model.hpp` the
way the device-authorization model does, and links only the core library.

**Seventeen intents, eight legal, nine refused before construction.** The
first eight of `anonymous_read_intent` mirror the credential-free read
operations one to one — repository, pull request, changed files, commits,
reviews, issue comments, checks, search — so the adapter maps its catalog
onto this policy without a second table. The other nine are refused with
six distinct typed reasons: review threads (`graphql_refused`, the one
GraphQL-routed operation), viewer identity (`viewer_identity_refused`),
the five mutations (`mutation_refused`), a private repository
(`private_or_missing`, one reason for both so the answer is
existence-neutral by construction), and an enterprise host
(`ghes_unavailable`). `catalog_index` places every enumerator in a switch
and a `static_assert` ties the catalog's size to the count, so an intent
added to the enumeration cannot reach a legality answer without being
placed.

**Every refusal offers sign-in, and only the budget refusal is terminal.**
`anonymous_refusal::SignInOffered` is `static constexpr true`: the bound
sign-in-handoff definition is a property of the type, not a flag a caller
sets. `refuse_rate_limited` carries `anonymous_rate_limit_fact{Limit,
Reset}`, both opaque — core copies the reset instant the adapter measured
and never orders or ages it — and `terminal()` is true for it alone, so
the policy states no retry past the limit.

**Unavailable is a state, never a default.** Viewer identity, thread
resolution, viewed-file state, actionability, and observation availability
are each a leading-`unavailable` enumeration equal to value-initialisation,
following `github_read_model.hpp`'s precedent; `inline_review_comment_observation`
distinguishes "not observable" from "observed none", which is the RFC's
"unavailable, not empty" row. `anonymous_capabilities()` is all
`unsupported` and `gated_intent` proves no capability-gated action is
legal.

**Identity and the mode switch.** `anonymous_request_identity` reuses
`pull_request_request_context` — the same `capability_revision`,
`github_identity_generation`, and `request_generation` the authenticated
path already carries — and `anonymous_read_policy_state::admit` rejects a
stale, cross-mode, regenerated, duplicate, or unrequested completion by
typed identity. `mode_change_invalidation` yields the
`invalidate_github_identity_request` the host already applies on an
account switch, and the test proves it equal in both directions to what
`transition(github_state, activate_host_profile_intent)` emits.

**The inventory script now matches the ledger's commands.** The RFC-0017
ledger's rows invoke `verify_rfc0017_test_inventory.py` with `--build-dir`,
`--required`, and `--all`, none of which the script accepted, and its single
graph anchor (the native HTTPS fixture) would have owed the core selector
nothing on a `core-only` graph. The script now anchors each selector on its
own sibling registration (`SELECTOR_ANCHORS`; the core selector is anchored
on nothing and owed by every graph), accepts the ledger's spellings, insists
under `--required` and `--all` rather than accepting an anchored zero, and
has a fifteen-case hermetic suite registered as
`rfc0017_test_inventory_contract`.

What the worker flagged, recorded rather than resolved: `legality(authenticated, ·)`
is legal for everything by design, since capability and permission gating
stay in `github_transition`; `Reset` is an opaque `std::uint64_t` whose
encoding the adapter chooses, so [[BPR-0301]] and [[BPR-0303]] must agree;
and [[BPR-0301]] must not read `pull_request_request_context::Account` to
infer the mode, which is value-initialised under public preview.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The policy suite and its two siblings pass, and both inventory gates with them | `ctest --preset dev -R '^(core_anonymous_public_read\|core_github_policy\|core_device_authorization\|rfc0017_test_inventory\|rfc0017_test_inventory_contract)$' --output-on-failure` | 5/5 passed: policy 0.06 s, github policy 0.06 s, device authorization 0.06 s, inventory 0.13 s, inventory contract 0.14 s | this commit |
| The architecture boundaries hold with the new core files | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on the header, source, and test | exit 0, no output | this commit |
| The ledger's `--all` form passes on the dev graph | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all` | `RFC0017_TEST_INVENTORY_OK` | this commit |
| The inventory's own refusal table | `python3 tests/scripts/verify_rfc0017_test_inventory_tests.py` | **15 tests, OK** | this commit |
| The refusal branch is load-bearing | The worker removed the review-threads refusal, rebuilt, and ran `core_anonymous_public_read`; then restored it | exit 8, `FAILED: every refused intent carries its own typed reason`; restored, 3/3 selectors pass | this commit |
| The ledger's exact core-only row | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/core-only --required core_anonymous_public_read && ctest --test-dir build/core-only --no-tests=error --output-on-failure -R '^core_anonymous_public_read$' && ctest --preset core-only --output-on-failure` | First run: `RFC0017_TEST_INVENTORY_OK`, the selector 1/1, then the complete matrix 111/112 in 285 s with `test_network_isolation_manifest` red at HEAD on its count bound — pre-existing, carded and pinned as [[BPR-0530]]. Rerun after the pin: **112/112 in 285 s** | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
