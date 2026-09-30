# Live target qualification milestone, macOS arm64, 2026-09-30

Coordinator-implementer milestone "live target qualification" (recipe
revision 2), after the [red-test backlog](red-test-backlog-2026-09-30.md).
Base: `b712704f9`. Station: macOS 26.6.2 arm64, dev preset (Debug, Apple
clang 21), Qt 6.11.2, Apple Git 2.50.1, `gh` 2.101.0 signed in as
`MikeMarcin`.

Owner designation, 2026-09-30, in reply to "BPR-1176 stays blocked until you
specify a safe GitHub target": "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". Remote mutations in this milestone were made
on that repository only. GHES remains undesignated.

## Cards

| Card | Outcome | Commit on main |
|---|---|---|
| [BPR-1176](../issues/60-done/BPR-1176-create-and-reconcile-the-resulting-pr-without-duplication.md) | Done. RC59-10's live course passes on BPR-Test for GitHub.com: server identities equal local ones (non-zero offsets kept, null-identity deletion), ready and draft PRs, lost branch and PR answers recovered by reading, a foreign branch refused, a raced branch and PR recorded as foreign. The product needed no change; the course found only its own fault-routing bug. GHES stays unadmitted | `4ce2d0529`, `9b344212a` |
| [BPR-1253](../issues/60-done/BPR-1253-gcc-15-release-reads-the-tour-submission-aggregate-as-uninitialized.md) | Done. The Linux station's last GCC 15 `-O3` failure, filled member by member. The file is in the round-trip contract's source-tree inventory only, not a pinned digest, so no reseal | `4169e0973` |
| [BPR-1243](../issues/60-done/BPR-1243-three-repository-contract-rows-are-red-at-main.md) | Evidence only: clang-format 23.1.1 accepts `fd05277e`'s `dispatch_tour_action` block; `coding-format-check` passes on 1159 files | `a3310a06a` |
| [BPR-1222](../issues/50-blocked/BPR-1222-qualify-live-github-review-tours.md) | Stays Blocked. Missing: a second (reviewer) GitHub account and an attended course | with this record |
| [BPR-0143](../issues/40-parked/BPR-0143-mixed-client-acceptance.md) | Stays Parked. Authority and a second client are now available; missing: an attended session observing the shipped workbench after each Refresh | with this record |
| [BPR-0144](../issues/40-parked/BPR-0144-acting-identity-on-a-real-account.md) | Stays Parked. Missing: a second GitHub account, since GitHub refuses Approve and Request Changes from the author | with this record |
| [BPR-0169](../issues/40-parked/BPR-0169-rfc-0010-observer-rules-leave-github-untouched.md) | Stays Parked. Missing: a PR on BPR-Test authored by another account with `MikeMarcin` neither author nor reviewer | with this record |

## Live objects on MikeMarcin/BPR-Test

All under `bpr-rc59-10/<stamp>/`, left as evidence; nothing was deleted,
closed or force-updated.

- `20260930T102712Z` (the recorded run): branches `base`, `foreign`, `ready`,
  `draft`, `lost-branch`, `lost-pull`, `raced-branch`, `raced-pull`; PRs
  #12–#16 (#13 draft; #16 is the "other actor's").
- `20260930T102240Z` (passing run before the course was committed): the same
  eight branches; PRs #7–#11.
- `20260930T102136Z` (stopped by the course's own fault): branches `base`,
  `foreign`, `ready`, `draft`, `lost-branch`, `lost-pull`; PRs #3–#6.

## Attended items

Session `cbd48b91000b00f1e72e1ad62c7c384c`: added `BPR-1176-rc59-10-scope`
(`9b344212a`), one Accept/Reject question on reading RC59-10's "per claimed
supported host" as the GitHub server, with the harness course and the owed
Linux and Windows runs. It is ready and unprotected. This milestone started
no server; a `serve` host for the session (pid 9702, port 59998) was already
running and was left alone.

## Checks

- Focused rows at the milestone's changes: the nine
  `application_review_tour_publication_*` rows,
  `application_review_tour_composition_public_path` and
  `application_review_round_trip_composition_contract` pass (11 of 11).
- With the live course built into `build/dev`: `architecture_boundaries`,
  `architecture_boundary_contract`, `test_network_isolation_manifest`,
  `test_network_isolation_structure_contract`,
  `live_mutation_isolation_contract`, the documentation rows,
  `review_cycle_publisher`, `github_publication_contract` and
  `native_https_transport_fixture` pass (10 of 10); with it switched back off,
  the architecture and isolation rows pass again.
- `coding-format-check` passes; `tidy.py` passes on the two changed C++ files.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; its 64 self-tests pass.

## Owed

- Linux station: BPR-1253's tests-off Release compile under GCC 15, and the
  RC59-10 driver with a `gh` sign-in that can push to BPR-Test (command in the
  [ledger](RFC-BPR-0053-0061.md)).
- Windows station: the RC59-10 driver with Git for Windows.
- Owner: a second GitHub account for BPR-0144, BPR-0169 and BPR-1222; an
  attended session for BPR-0143 and BPR-1222; a designated GHES target before
  GHES is admitted for native publication; the verdict on
  `BPR-1176-rc59-10-scope`.

## Linux station, 2026-09-30

bpr-main-linux, Ubuntu 26.04 on WSL2, GCC 15.2, at `4b6592d83`:

- BPR-1253: the tests-off Release graph compiles with no failure and no
  warning. `application_review_round_trip_composition_contract` passes.
  `application_composition_contract` passes when run directly (8 tests,
  672 s), but under ctest it exceeds its 600 s limit on this machine.
- RC59-10: the driver passes live on `MikeMarcin/BPR-Test` through the
  OpenSSL transport and Git 2.53.0 (run `20260930T142456Z`, PRs #17 to #21).
  The owner confirmed this run in the station's session. The Windows run is
  still owed.

## Windows station, 2026-09-30

bpr-main-win. RC59-10: the driver passes live on `MikeMarcin/BPR-Test`, run `20260930T185949Z`, PRs #22 to #26. It went through Schannel and Git for Windows 2.55.0, and the operator confirmed the run in the station's session. The only change is the driver's own Windows cleanup fix (`f8d90fad`). The details are on [BPR-1176](../issues/60-done/BPR-1176-create-and-reconcile-the-resulting-pr-without-duplication.md). With the Linux run, no station run of RC59-10 is owed.
