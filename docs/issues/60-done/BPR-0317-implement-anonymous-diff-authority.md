---
schema: "implement-rfc.card.v1"
id: "BPR-0317"
title: "Implement anonymous Git cache epoch and diff authority"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0301","BPR-0303","BPR-0531"]
---

# Implement anonymous Git cache epoch and diff authority

## Required final state

- Public GitHub.com smart HTTP uses explicit credential-free authority with
  zero Authorization/vault effect and the existing trust/proxy/redirect limits.
- A discriminated repository authority replaces the live path's assumption
  that every cache/catalog/epoch subject is a nonzero viewer/account: the
  authenticated branch retains its exact account identity, while the anonymous
  public branch carries only canonical GitHub.com repository/profile identity.
  No zero/default/synthetic account is representable, and the two namespaces
  cannot alias or leak objects or projections.
- The SQLite epoch catalog receives a versioned authority discriminator and a
  transactional schema migration that maps every existing row to the
  authenticated branch without changing its account/repository identity.
  Unknown versions, malformed discriminators, partial migration, substitution,
  collision, downgrade, and mixed-authority rows fail closed.
- The selected file reaches the existing diff computation and Qt projection
  with exact repository/path/base/head/content identity and bounded cancellation.
- Private/GHES/mutation/push, stale switch, malformed pack, substitution,
  cache-reuse, and authenticated non-regression tests fail closed.
- Exact core/application/Git/SQLite tests plus a real installed-Git and SQLite
  two-process restart course prove migrated authenticated rows remain readable,
  anonymous rows reopen without viewer identity, mode switching never reuses the
  other namespace, and no credential is discovered or persisted.

## Lane — 2026-09-02

[[BPR-0301]], [[BPR-0303]], and [[BPR-0531]] are terminal; claimed straight
into `30-in-progress/` by the integrator, its worker started in a worktree on
the head that carries all three, under the transition exception. What they
leave at this card's door: core's `pull_request_authority.Mode` (zero account
and viewer under public preview by rule; the comparison digest appends the
mode byte only for a non-authenticated authority so persisted anchors keep
matching — making that unconditional belongs with a domain bump and a
migration here); the application's `epoch_catalog_authority_key` and epoch
handoff records carrying `Mode` with the same two-rule validity; a host that
under public preview opens no epoch catalog and no Git cache and reports the
handoff `unavailable` through `epoch_handoff_unavailable`; and a transport
whose `can_acquire_smart` refuses every credential-free binding, so public
smart HTTP needs its own typed binding and factory beside
`anonymous_read_request`.

## Part one landed — 2026-09-02

Integrated at `64dde02f` with the card open; the host half follows as part
two.

**The binding.** `anonymous_smart_request` is a fourth alternative beside
`anonymous_read_request`, admitted by `can_acquire_smart` only when the
smart request context's authority is `public_preview` on a
`github_com_2022_11_28` profile; `make_smart_request` attaches Basic only
under `carries_authorization()`, and `build_smart_url` emits only
`git-upload-pack`, so there is no push route to refuse at run time. The
account binding now also requires an authenticated context — before this
slice an anonymous authority could have leased an authenticated transport,
which nothing had asked. `make_native_https_anonymous_smart_transport` ships;
its `_for_test` twin exists only under `BPR_NATIVE_HTTPS_TESTING`, and the
release-absence inventory forbids it as symbol and string while recording
the shipped factory as known product code.

**The cache manifest.** `manifest_authority_bytes` appends the mode byte for
a non-authenticated authority — core's own `append_authority` rule — so
every authenticated manifest is byte-identical to before and an anonymous
scope for repository R is a different scope from the authenticated one.
The worker's negative mutation showed the guarantee rests on a pair:
removing the mode byte alone still refuses aliasing because the account
already differs, removing the account alone still refuses because the mode
differs, and only removing both lets a signed-in authority reuse a
signed-out scope. `core::valid` forbids two valid authorities differing
only in mode, so the pair is the honest statement of it.

**The catalog.** Schema 2: `access_mode` on `repository_scope` and
`pull_request_scope`, CHECK constraints tying a zero account or viewer to
the public-preview mode so a zero is admitted only under the typed mode and
never as a sentinel, `repository_scope_active_authority` and the pull-request
scope's parent key carrying the mode so a mixed-authority row is
structurally impossible, and a v1→v2 migration that rewrites every existing
row authenticated in one transaction, verifies counts, and rolls back when
interrupted before commit (a `BPR_SQLITE_TESTING` hook,
`interrupt_next_migration_before_commit`, proves it). Unknown, negative, and
future versions are refused, never recreated. `read_u64_including_absent`
lets a public-preview row carry its zero viewer where `read_u64` rejects zero
by design.

**Two selectors.** `git_anonymous_public_read` and
`sqlite_anonymous_epoch_authority`, both registered uncontained on Windows
under the causes their siblings record and pinned in the network-isolation
manifest; both allocated in the inventory, anchored on `git_cache_contract`
and `sqlite_epoch_catalog_storage_contract`.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | `64dde02f` + this commit |
| The two new selectors, every Git, smart-HTTP, catalog, ledger, epoch, diff, and host sibling, the manifest and inventory gates, and both composition contracts | `ctest --preset dev -R '^(git_anonymous_public_read\|sqlite_anonymous_epoch_authority\|git_process_contract\|git_cache_contract\|native_git_smart_transport_(contract\|fixture)\|native_https_anonymous_read_(contract\|fixture)\|sqlite_epoch_catalog_(storage\|behavior)_contract\|sqlite_review_ledger_storage\|sqlite_review_mutation_recovery_storage\|application_epoch_[a-z_0-9-]*\|application_diff_host\|application_anonymous_read_host\|application_github_host\|core_epoch_policy\|core_diff_model\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_release_absence_negative_control\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 29/29 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the ten touched files | exit 0, no output | this commit |
| The shipped image carries the smart factory and not its test twin | `cmake --build --preset app-release`, then `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `RFC0017_TEST_INVENTORY_OK`; `nm -C` finds 2 `make_native_https_anonymous_smart_transport` symbols and 0 `_for_test`; the dev binary as a positive control names the new seam twice | this commit |
| The manifest gate's and the inventory's own contracts | `python3 tests/scripts/verify_test_network_isolation_tests.py`; `python3 tests/scripts/verify_rfc0017_test_inventory_tests.py` | OK; OK | this commit |

## Part two landed — 2026-09-02

Integrated at `283212ae`. With it the card's required final state is met and
it closes.

**The host delivers the handoff.** `github_application_host` gains one
trailing defaulted port, `git_smart_transport_factory_port* aAnonymousSmartGit`
— one port rather than the two the design named, because the host consumes
no Git cache port on either path: the cache is opened downstream by
`epoch_application_host` from the handoff's authority, so a cache port on
the GitHub host would have been an unused parameter. Under public preview
with the port present, `make_epoch_handoff` builds a `public_preview`
authority with zero account and viewer and delivers it;
`deliver_epoch_handoff` reports `epoch_handoff_unavailable` only when the
port is null, which is today's composition until [[BPR-0305]] wires
`make_native_https_anonymous_smart_transport`.

**Discrimination reaches the cache.** `git_smart_authority_binding` carries
`Mode`, `agrees()` compares it, and `valid()` for the binding and for
`accepted_pull_request_handoff` is mode-discriminated — public preview: zero
account and viewer and no current viewer review; authenticated: unchanged.
In `epoch_application_host`, `catalog_key`, `agrees(authority, handoff)`, and
`invalidation_identity` carry `Mode`, which is what gives a signed-out read
its own catalog and cache scope through `git_cache_open_request.Authority`
(part one's mode-scoped manifest). A mode switch invalidates through the
existing path and reaches the epoch authority.

**`application_anonymous_diff_authority`.** Five cases: the host under public
preview with the anonymous transport port renders the selected-file diff
through `diff_application_host` under a `public_preview` authority with zero
account and viewer; the same pull request under an authenticated session gets
a different authority and cache scope; a mode switch invalidates the epoch
authority; zero vault, credential, or `/user` calls; with the port null the
handoff stays typed-unavailable with zero requests. Registered like its
siblings, with no isolation keyword. The rendered diff is driven with the
diff course's fake blob bytes rather than a local fixture repository —
real-repository rendering under an anonymous authority is the adapter
courses' evidence here and the platform course's ([[BPR-0306]]) end to end.

**The wire.** `native_git_smart_transport_fixture` gains three anonymous
scenarios — discovery, ls-refs, fetch — driven by the `_for_test` smart
factory: each asserts one connection, one origin, exactly one
`git-upload-pack` target and zero `git-receive-pack`, and the loopback proxy
raises `anonymous-authorization` or `anonymous-credential-origin` if any
`Authorization` header or credential-derived byte appears. The driver still
feeds the credential frame on the anonymous course, so the absence is proved
rather than arranged. The GHES refusal before the wire sits in
`native_git_smart_transport_contract`. No new loopback registration.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The new selector, the anonymous host, every epoch and diff host course, the smart-HTTP contract and fixture with the anonymous scenarios, the two part-one selectors, core's epoch policy, the manifest and inventory gates, and both composition contracts | `ctest --preset dev -R '^(application_anonymous_diff_authority\|application_anonymous_read_host\|application_epoch_[a-z_0-9-]*\|application_diff_host\|application_github_host\|native_git_smart_transport_(contract\|fixture)\|git_anonymous_public_read\|sqlite_anonymous_epoch_authority\|core_epoch_policy\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 20/20 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the six touched files | exit 0, no output | this commit |
| The ledger's `--all` inventory form with the new selector allocated | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all` | `RFC0017_TEST_INVENTORY_OK` | this commit |
| The anonymous handoff cannot wear an account | The worker made the public-preview handoff carry an authenticated-mode account (`51`), rebuilt, ran `application_anonymous_diff_authority`; restored from a byte copy | `FAILED: the anonymous handoff is delivered rather than failed`; restored, passes | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
