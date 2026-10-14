---
schema: "implement-rfc.card.v1"
id: "BPR-1494"
title: "A returning account does not resume its retained comparison"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# A returning account does not resume its retained comparison

`application_recent_account_isolation` fails on main at cd4860e30, in the
full gate and again alone (49 s), at its identity seed:

```text
pending=returning account resumes its exact retained comparison status=6 diagnostic= recording=15 operation=8
FAILED: returning account resumes its exact retained comparison
identity-seed failed (1)
```

It also failed in the 2026-10-12 gate. BPR-1403's ledger records an earlier,
different cause (a fresh legacy phase without a sign-in, fixed in ab32dd295).

## Acceptance

- AC1: The cause is named with the commit that introduced it, and it is
  classified as a product defect or a course defect, with the evidence.
- AC2: The fix makes `application_recent_account_isolation` pass on macOS
  without weakening account isolation or the exact-comparison check.

## Boundaries

Account isolation semantics do not change.

## Evidence

**Cause (AC1): product defect, introduced by 268b87049 (BPR-1409).** That
commit added `native_sign_in_profile` to `src/app/src/github_application.cpp`.
It mints a new host profile for every native sign-in that begins on a
remembered native profile, because the account is unknown when the ID is
chosen and core's update rule binds one profile ID to one account. In the
identity seed, 910001 signs in on profile 1, 910002 on profile 2, and the
returning 910001 on profile 3. The saved reading's exact comparison is scoped
to profile 1, because the pull-request authority scope includes profile,
revision and host. `window_reading_restoration::advance` therefore correctly
refuses it as unavailable, and the window lands on Home. The course is right:
RFC-BPR-0071 asks that a returning account's exact retained state resume.

**Fix (AC2): the owner's option (a), which decides the identity after `/user`
names the account.**

- Core. `adopt_native_connection` returns the account's own earlier native
  identity only for a provisional sign-in profile above `LastProfileId`. The
  earlier identity must have the same host ID, hostname, server, numeric
  account and login, and may be selected or retired. `remembered_connection_state::Retired`
  keeps up to 16 native identities that another account's native selection
  displaced. `valid_remembered_connection_update` admits a retired identity
  only exactly as it was retired, and refuses rewritten history. A CLI
  selection or Forget drops the server's retired identities.
- Application host. `enroll_profile` takes an optional re-bind answer. After
  the bootstrap `/user`, and before any vault write, a different profile
  restarts the same material there, and `/user` must name the same account or
  the session fails with the account-mismatch diagnostic.
- Composition. The device and manual native enrollments ask
  `rebind_native_sign_in`, which applies core's answer. If the profile is not
  registered, because startup rebuilds only the selected profile, it registers
  the retired identity again exactly as it was. It also moves the device
  capability.
- SQLite. Schema version 2 adds `retired_connection`, and an admitted version
  1 file is upgraded in place.

No restoration or exact-scope check changed. A different account never
re-binds, and a native sign-in that replaces CLI still receives a fresh
identity (`tests/end_to_end/remembered_connection_public_path_tests.cpp:317`,
in `application_remembered_connection_public_path`).

**Regression tests.** `core_remembered_connection_policy` gains
`returning_native_account_rebinds`. `sqlite_remembered_connection_storage`
gains `retired_identities_and_upgrade` and refuses a retired identity carrying
another account.

**Before and after.**

- Core rule probe: three `remember_connection` calls (910001, then 910002,
  then 910001 again on profile 1), compiled against main's
  `remembered_connection.cpp` and this branch's. Before:
  `returning-account-rebind=refused`, exit 1. After: `durable`, exit 0. The new
  core test does not compile against main, which has no
  `adopt_native_connection` or `Retired`.
- `application_recent_account_isolation` with re-binding disabled (a temporary
  early return in `rebind_native_sign_in`) fails with this card's signature:
  `pending=returning account resumes its exact retained comparison status=6
  diagnostic= recording=15 operation=8`, `identity-seed failed (1)`. With the
  fix it passes all four phases, each `retained_packs=2 retained_epochs=2
  retained_anchors=4 unchanged=True`, and ends with `OFFLINE_INVENTORY_OK`.

**Rows run (macOS arm64, dev preset, final build).**
`ctest -R 'remembered_connection|device_authorization|sign_in|signin|account|profile|restoration|window_review|recent|core_dependency_boundary|architecture_boundar'`
passed 64 of 64, including `core_dependency_boundary`,
`architecture_boundaries`, `architecture_boundary_contract`,
`core_window_review_restoration`, `core_remembered_connection_policy`,
`application_remembered_connection_host`,
`application_window_review_restoration`,
`sqlite_remembered_connection_storage`, the device-authorization courses,
`application_profile_two_account_isolation_public_path`,
`application_recent_account_isolation`,
`application_remembered_connection_public_path` and
`application_remembered_connection_restart`. The host rows
`application_github_host`, `application_github_contract`,
`application_github_credential_vault`, `application_github_window_read_host`,
`application_github_capability_cache`, `application_anonymous_diff_authority`
and the four `application_github_public_path_*` rows passed 10 of 10. The
documentation gate and a clang-format 22 check of the changed files passed.
The `core-only` preset passed 276 of 276. `rfc0016_production_identity_security`
first refused a temporary `.agents` link outside the repository, then passed
once the link was removed. clang-tidy 22 reports nothing on the changed
lines. Its remaining findings in `github_application.cpp` are pre-existing
(lines 8439, 9741, 10453 and 29486 to 29672).

**Limits.** The staged setup-destination enrollment (a pull-request link for
a server without a sign-in) does not re-bind yet. CLI accounts still receive a
fresh identity on rediscovery, which is the BPR-1502 family. Retired identities
are bounded at 16, oldest dropped.
