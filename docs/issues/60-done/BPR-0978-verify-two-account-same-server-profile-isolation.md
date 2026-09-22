---
schema: "implement-rfc.card.v1"
id: "BPR-0978"
title: "Verify concurrent two-account same-server profile isolation"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0693", "BPR-0694", "BPR-0695", "BPR-0696", "BPR-0701", "BPR-0702"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
acceptance: [{"criterion":"Both same-server fixture accounts remain concurrently usable, with distinct exact account authority and profile-scoped vault/storage identities.","outcome":"pass","revision":"6f8e44e4","evidence":{"kind":"artifact","detail":"application_profile_two_account_isolation_public_path: Default and Work run concurrently on one registry root, sign in as @octo-reviewer (1001) and @other-profile-reviewer (1002), each vault enrolls exactly one credential, both open the same pull request through their own account transports; 13.06 s pass, build/bpr0978-focused.log.","path":"docs/issues/30-in-progress/BPR-0978-verify-two-account-same-server-profile-isolation.md#implementation--2026-09-22"}},{"criterion":"Writes/reads to repositories, caches/epochs, private review state and preferences discriminate the two profiles, including the same PR identity.","outcome":"pass","revision":"6f8e44e4","evidence":{"kind":"artifact","detail":"Reviewed-head rows are read directly from each root's state/v1/review-ledger.sqlite3: Work's row only under profiles/profile-2 with account 1002, Default's only under the registry root with 1001, for the same pull request; Work's accepted appearance persists through Work's host while Default stays dark. Negative control replacing the ledger scope account with a constant fails the exact-account row (build/bpr0978-negative.log).","path":"docs/issues/30-in-progress/BPR-0978-verify-two-account-same-server-profile-isolation.md#implementation--2026-09-22"}},{"criterion":"Sign-out/replacement/late completion in one profile does not retire, expose or mutate the other profile; a deliberately broken scoping dependency is detected by the public course or focused boundary regression.","outcome":"pass","revision":"6f8e44e4","evidence":{"kind":"artifact","detail":"With a Work refresh terminal held, Work signs out from its server card, its credential is erased and session retired, releasing the terminal cannot revive it; Default's vault slot, read count, review, account and preferences are unchanged and Default still refreshes; closing Work leaves Default usable. The ledger-account mutation is the demonstrated detection.","path":"docs/issues/30-in-progress/BPR-0978-verify-two-account-same-server-profile-isolation.md#implementation--2026-09-22"}},{"criterion":"Relevant public, credential and storage tests pass without live writes.","outcome":"pass","revision":"6f8e44e4","evidence":{"kind":"artifact","detail":"New course plus application_profile_startup_public_path, application_profile_multi_window_public_path, application_github_public_path_github_com and test_network_isolation_structure_contract pass (build/bpr0978-focused.log); test_network_isolation_manifest fails only on main's pre-existing BPR-0962 rows. Format check and documentation gate pass; clang-tidy on the changed TUs reports only pre-existing findings outside the changed lines (build/bpr0978-tidy.log). No live credential or provider write.","path":"docs/issues/30-in-progress/BPR-0978-verify-two-account-same-server-profile-isolation.md#implementation--2026-09-22"}}]
---

# Verify concurrent two-account same-server profile isolation

## Scope

Complete RFC-0024 AC-02/08/09 through the shipped public composition using
two concurrently open BPR profiles with distinct fixture accounts on the same
server. Discriminate credentials, repositories/recents, private roots/caches,
epochs, notes/drafts/receipts, preferences and sign-out/late completion. Repair
any demonstrated product boundary defect; do not add live credentials or
network mutations.

Owned surfaces: public-path fixtures and end-to-end course/contracts, plus
exact demonstrated defects in profile composition, scoped host authority,
credential/storage adapters. Existing single-profile and isolated store
evidence supplements but cannot replace the two-account composed course.

## Acceptance

- Both same-server fixture accounts remain concurrently usable, with distinct
  exact account authority and profile-scoped vault/storage identities.
- Writes/reads to repositories, caches/epochs, private review state and
  preferences discriminate the two profiles, including the same PR identity.
- Sign-out/replacement/late completion in one profile does not retire, expose
  or mutate the other profile; a deliberately broken scoping dependency is
  detected by the public course or focused boundary regression.
- Relevant public, credential and storage tests pass without live writes.

## Evidence

Baseline: the existing courses prove same-account sibling-window isolation,
not the concurrent two-account criterion.

### Implementation — 2026-09-22

Candidate: working tree over claim `cd386ce6`, the Codex owner's uncommitted
fixture start (a `DistinctProfileAccounts` option that substitutes a distinct
account id and login for every non-Default profile) adopted and completed. The
substituted viewer now changes database id, node id and login together: with
only the login changed, the recorded octo-reviewer records shared the viewer's
node id under a different login, and the read host's unique-identity
validation refused the whole read (`invalid_relationship` /
`relationship_actor_login` at SearchPullRequests in Work's diagnostics log).
That refusal is the product's, kept; the fixture now serves a coherent second
actor. No product boundary defect was demonstrated.

New composed course `application_profile_two_account_isolation_public_path`
(`tests/end_to_end/profile_two_account_isolation_public_path_tests.cpp`):
Default and a seeded Work profile start concurrently on one registry root
through the fixture start used by the startup course, each restoring its own
seeded appearance; both sign in from the Home landing card and the strip names
`@octo-reviewer` in Default and `@other-profile-reviewer` in Work; each vault
holds exactly one enrolled credential; both open `octo-org/review-repository`
pull request 17 through their own account transports; Work marks a reviewed
head and the row appears only in `profiles/profile-2/state/v1/review-ledger.sqlite3`
under account `1002`, then Default marks its own and its row appears only in
the registry root's ledger under `1001`; Work's accepted high-contrast
appearance persists through Work's host while Default stays dark; with a Work
refresh terminal held, Work signs out from its server card, its credential is
erased and its session retired, and releasing the held terminal cannot revive
it; Default's vault slot, read count, review, account and preferences are
unchanged and Default still refreshes afterwards; closing Work leaves Default
usable. Pass, 13.06 s standalone.

Negative control: replacing the review ledger scope's account with the
constant `"1001"` in `compose_review_ledger` fails the course at "Work's
reviewed head lives in Work's ledger under Work's exact account"
(`build/bpr0978-negative.log`, 12.10 s); the source is restored before the
final checks. The partition the course checks is the product's, not the
fixture's.

Focused regression (`build/bpr0978-focused.log`): the new course,
`application_profile_startup_public_path` 2.35 s,
`application_profile_multi_window_public_path` 77.82 s,
`application_github_public_path_github_com` 11.72 s and
`test_network_isolation_structure_contract` pass. `test_network_isolation_manifest`
fails on main's pre-existing `release_update_native_https` loopback-count row
and the `gh_cli_git_smart_transport` nested-sandbox row, already recorded on
[[BPR-0962]]; the same test fails identically in the main checkout's build and
names nothing from this card. The new row is registered `WINDOWS_UNCONTAINED`
with a `dos-volume-name` pin reasoned from the BPR-0806 profile rows, not yet
measured contained on Windows.

Format check passes for the changed C++ files (the fixture's three pre-existing
misaligned lines from the staging helper are corrected); the documentation
gate passes (`build/bpr0978-docs.log`). The authority inventory records the
course. No live credential, provider write or vault operation was used.
