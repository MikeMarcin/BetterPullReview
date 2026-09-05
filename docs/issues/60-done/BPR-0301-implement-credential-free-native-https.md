---
id: BPR-0301
title: Implement typed credential-free native HTTPS reads
status: Done
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0312]
---

# Implement typed credential-free native HTTPS reads

## Required final state

- Native HTTPS accepts an explicit anonymous read authority distinct from a
  missing credential and structurally omits authorization and all vault access.
- The existing authenticated transport and RFC-BPR-0002 TLS, proxy, redirect,
  cancellation, timeout, and response-limit rules remain unchanged.
- Provider admission refuses GraphQL, viewer identity, all mutations, private
  intent, and GHES before constructing a request.
- Loopback positive and hostile tests prove header absence with canaries and
  prove that absent/invalid authenticated credentials do not become anonymous.

## Lane — 2026-09-02

[[BPR-0312]] is satisfied: the owner's verdict validated on 2026-09-02 and
the four public-preview definitions are bound in
`docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json`.
The fourth is this card's admission rule stated from the product side:
GitHub.com public repositories only, every GHES profile unavailable with
zero requests. [[BPR-0035]] is terminal, so the RFC-BPR-0002 transport this
card extends is closed and its TLS, proxy, redirect, cancellation, timeout,
and response-limit rules are the fixed baseline. Both dependencies hold, so
this card moves to `20-ready/` as generic work runnable on any station.

## What this slice delivered — 2026-09-02

**A fourth binding, distinct in type from both a missing credential and the
device-authorization one.** `anonymous_read_request` sits beside
`credential_free_request` in `src/adapters/github/src/native_https_transport.cpp`,
file-private like it, carrying a session context and no material field. Its
`credential_matches(github_wire_request)` branch is the same in shipped and
test builds: the parent's session and identity generation must equal the
binding's, a pull-request parent must carry the profile's host and is not
compared by account — anonymous authority has none, and nothing reads
`Account` to infer the mode — and the operation must be one of the eight
credential-free reads. `list_review_threads` and `get_viewer_identity` are
refused by operation before any request exists. The device-authorization
overload does not match it; `can_acquire_smart` now requires
`carries_authorization()`, so public smart HTTP is refused typed until
[[BPR-0317]] opens it; the mutation claim still requires an account
credential.

**No header by construction.** The Bearer guard is `carries_authorization()`,
false for both credential-free bindings. The worker's negative mutation —
dropping the anonymous term so the binding attaches `Bearer` — built clean
and made `native_https_anonymous_read_fixture` fail with
`anonymous-read-proxy-contract`; restored from a saved copy, rebuilt, green.

**Factories.** `make_native_https_anonymous_read_transport(profile, context)`
ships; its `_for_test` twin exists only under `BPR_NATIVE_HTTPS_TESTING`.
Both take no credential parameter and both refuse any profile whose contract
is not `github_com_2022_11_28` with
`native_https_configuration_failure::anonymous_read_contract_unsupported`
before a transport exists, which is the GHES zero-request rule at the
adapter. They reuse the implementation constructor, so system trust, proxy
discovery, redirect refusal, deadlines, and response bounds are the shipped
ones rather than a copy.

**Provider admission.** `src/adapters/github/src/github_anonymous_read_admission.hpp`
holds `github_provider_read_authority{authenticated, anonymous_public}`,
`anonymous_read_refusal{identity_requires_credential, graphql_surface_unavailable, server_contract_unsupported}`,
and three `constexpr` functions: `anonymous_read_eligible` (an exhaustive
switch, so a new operation cannot default into the eligible set),
`admit_anonymous_read(operation, server_contract)` (the contract is checked
first, so a GHES profile is refused for every operation), and
`provider_failure_for` (projected once onto `authentication`,
`unsupported_capability`, `api_version`). `make_github_provider` takes the
authority on both overloads, defaulting to authenticated; the transport
factory port gains a virtual `make_anonymous` that defaults to a typed
refusal so existing fakes compile; `perform_paged_github_read` takes the
authority and `provider_request_valid`'s account-transport term is
authority-specific, the authenticated path verbatim. An anonymous
`fetch_pull_request` requires an all-false `CapabilityEvidence.ReadShape`
and refuses otherwise, which is what keeps the GraphQL body from being
compiled on the shared path.

**Three selectors, named as the ledger names them.**
`native_https_anonymous_read_contract` (every Qt graph, `--contract`): the
binding's match table over all ten operations, session/generation/host
mismatches, the device overload, smart HTTP, the mutation claim, the GHES
factory refusal with no transport constructed, and invalid account material
that does not degrade. `native_https_anonymous_read_fixture` (`--suite
anonymous_read`, the APPLE/Linux loopback block): a REST read succeeds with
no `Authorization` against a fixture that refuses any; the positive canary
sees the header through an account transport; refused operations never
reach the wire, by the fixture's exact request count; the fixture's leaf
certificate now also names `api.github.com` so the course runs on the
GitHub.com authority. `github_anonymous_read_contract`: the admission table
over all ten operations, GHES with zero transport requests, one eligible
operation reaching the anonymous transport with no `Authorization`, and every
eligible operation's compiled wire request carrying none.

**Integrator additions.** `scripts/verify_rfc0017_test_inventory.py`
allocates the three selectors — the two contract suites anchored on
`github_read_contract` and owed by every Qt graph on every platform, the
fixture anchored on the loopback fixture in the wire block — with
`--contract` and `--suite anonymous_read` as required arguments, forbids
`make_native_https_anonymous_read_transport_for_test` as symbol and string,
and records the production factory as known product code. Its hermetic
suite grew to seventeen cases. `test_network_isolation_manifest` was red on
the first reproduction — `native_https_anonymous_read_fixture has loopback
count 2, expected 0` — because the loopback-authority table is an
independent record; it is pinned at two, with the reason beside it.

Flagged, not resolved: the read-authority parameter on
`perform_paged_github_read` and the authority-specific
`provider_request_valid` are [[BPR-0302]]'s merge point; the all-false
read shape an anonymous session must present is [[BPR-0303]]'s to satisfy
from the host; `github_anonymous_read_contract` observes one eligible
operation on the wire because the recording transport has no snapshot
corpus, and an eight-on-the-wire course wants the fixture corpus
[[BPR-0302]] builds.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean with the new adapter files | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The three new selectors, every native-HTTPS and GitHub-contract sibling, the provider and pager suites, the manifest gate, and the pinned round-trip contract | `ctest --preset dev -R '^(native_https_anonymous_read_(contract\|fixture)\|github_anonymous_read_contract\|rfc0017_credential_free_native_https\|native_https_transport_fixture\|native_https_device_authorization_(contract\|fixture)\|github_read_contract\|github_device_authorization_contract\|github_mutation_contract\|github_paged_read\|github_provider\|github_capability_probe\|native_git_smart_transport_(contract\|fixture)\|github_device_authorization_refresh_contract\|test_network_isolation_manifest\|application_review_round_trip_composition_contract)$' --output-on-failure` | 17/18 on the first run — the manifest gate red on the unpinned loopback count, every other selector green: anonymous contract 1.26 s, anonymous fixture 0.90 s, provider contract 0.21 s, round-trip contract 33.79 s. After the pin the manifest gate passes in 2.99 s | this commit |
| The composed application still admits its fixtures end to end | `ctest --preset dev -R '^application_composition_contract$' --output-on-failure` | Passed in 121.83 s | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on the ten touched or new C++ files | exit 0, no output | this commit |
| The ledger's `--all` form and the three registered inventory gates | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all`; `ctest --preset dev -R '^(rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_release_absence_negative_control)$'` | `RFC0017_TEST_INVENTORY_OK`; 3/3 passed (0.12 s, 0.12 s, 1.43 s) | this commit |
| The inventory's own refusal table | `python3 tests/scripts/verify_rfc0017_test_inventory_tests.py` | **17 tests, OK** | this commit |
| The shipped image carries the production factory and not the test seam | `cmake --preset app-release && cmake --build --preset app-release`, then `python3 scripts/verify_rfc0017_test_inventory.py --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | build exit 0; `RFC0017_TEST_INVENTORY_OK` against the 10,468,832-byte artifact (sha256 `042d2fad…`); `nm -C` finds 2 `make_native_https_anonymous_read_transport` symbols and 0 `_for_test` | this commit |
| The absence scan can fire on the new seam | the same scan against `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | exit 1: `symbol present in release binary: make_native_https_anonymous_read_transport_for_test` and the matching string, beside the two pre-existing seams | this commit |
| The manifest gate's own contract | `python3 tests/scripts/verify_test_network_isolation_tests.py` | **37 tests, OK** | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
