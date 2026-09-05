---
schema: "implement-rfc.card.v1"
id: "BPR-0543"
title: "The Windows Credential Manager vault stores refresh material in a real second slot"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The Windows Credential Manager vault stores refresh material in a real second slot

## Why

The RFC-BPR-0016 closeout review of the platform vaults on 2026-09-03
([[BPR-0285]], the integrator's own reading before the third independent
review) found that `src/adapters/github/src/windows_credential_manager.cpp` implements the refresh slot as three
stubs: `store_refresh`, `retrieve_refresh`, and `delete_refresh` each answer
`github_credential_vault_failure::unavailable` without touching the
platform store (its own comment says the slot "has no Credential Manager target of its own yet" and that enabling it "belongs to BPR-0311", which closed without doing so). The recorded distributable registration issues
expiring user tokens with refresh tokens
(`registration-observation.json`: `user_access_token_expiration_enabled:
true`, `refresh_policy: expiring_user_token_with_refresh`), so a real
sign-in on Windows runs this path: the grant parks its refresh token in
memory, the `/user` observation binds the authority and flushes it
(`bind_refresh_authority`), the store answers `unavailable`,
`on_refresh_stored` calls `fail_storage`, and the host reports
`reenrollment_required` with `storage_failed` on the sign-in panel a
moment after the sign-in succeeded. The access token then expires after
its lifetime with nothing to renew it. No course sees this: every enabled
platform course stops at `cancelled` before a grant, and every composed
course runs over the fixture vault, which has a real refresh slot. The
platform table's "Enabled since [[BPR-0311]]" row and the ledger's
refresh-lifecycle claim therefore overstate Windows, and RFC-BPR-0016
cannot close until the slot is real here.

## Required final state

- `src/adapters/github/src/windows_credential_manager.cpp` stores, retrieves, and deletes refresh material in a
  second slot of the same platform store, keyed by the same authority as
  the base credential and under a distinct target so one slot can never
  answer for the other — the shape `macos_keychain.cpp` gives its
  `RefreshBackend` and its `github-native-refresh` service. `unavailable`
  is answered only when the platform store itself is unreachable, never as
  a stand-in for an unimplemented slot.
- `windows_credential_manager_contract` gains the refresh-slot cases the macOS contract has
  (`test_the_refresh_slot_reaches_only_its_own_service` and the store,
  retrieve, absent, and delete answers), and `windows_credential_manager_native` exercises the
  slot against the real Credential Manager on the station: a stored refresh entry
  is retrievable under its authority, is absent from the base slot, and is
  gone after `delete_refresh`. Each new case fails against the stub before
  the change; record that run as the negative control.
- Erasure honesty: with [[BPR-0540]] deleting the refresh slot beside the
  base credential, an orderly shutdown on Windows leaves no refresh
  material in the store; the native test proves the delete answers success
  for an absent entry rather than `unavailable`.
- The card records the host, toolchain, the exact selectors run, and
  results; format and strict tidy pass on the touched files.

## Non-goals

- Any change to the device host, the composition, the macOS vault, or the
  refresh lifecycle's timing.
- Persisting a session across a restart ([[BPR-0541]]).

## Lane — 2026-09-03

Carded by the integrator straight into `21-ready-win/` with no open dependency:
the slice is Windows-native (the platform store and its native test)
and is dispatched to `bpr-win` on `windows-lane`. The integrator merges the
lane, then the RFC-BPR-0016 final-head replays run at the head that
carries it.

## The slot is real — 2026-09-03

Implemented on `windows-lane` at `8f9d5616` (`main` `de172d04` merged into the
lane). The three stubs are gone: `store_refresh`, `retrieve_refresh` and
`delete_refresh` now reach a second Credential Manager target through the same
queue, the same effect lane, and the same authority account as the base
credential.

### Host and toolchain

```text
Windows 11 Home 10.0.26200 (x64), the JaegAW host
Visual Studio 18 Community, MSVC 14.51.36231, x64 Native Tools (vcvars64)
CMake 4.4.2   Python 3.14.7 (py -3)   Qt 6.10.3 (msvc2022_64)
clang-format / clang-tidy 22.1.8
```

Under the [[BPR-0493]] procedure, with Git's `usr/bin` appended to `PATH` for
`perl` per the [[BPR-0036]] addendum — appended, never prepended, because it
also ships a `link.exe` and `check_platform_surfaces` shells `link /MAP`.

### The shape

`macos_keychain.cpp`'s, deliberately, so the two platforms answer the same way:

- `windows_credential_manager_target_scope` gains `production_refresh` and
  `integration_test_refresh`. Their prefixes are
  `org.betterpullreview.github-native-refresh.windows-credential-manager.v1/`
  and its `integration-test` sibling — a different prefix, the same encoded
  authority account, so both slots key the same authority and neither target
  can name an entry in the other. Static assertions pin both lengths (647 and
  664 UTF-16 code units, inside `CRED_MAX_GENERIC_TARGET_NAME_LENGTH`) and
  assert that neither prefix is a prefix of the other.
- A `credential_slot` rides on the queued operation rather than being derived
  from the context, so queued work cannot be re-routed after the fact;
  `execute` picks `RefreshBackend` or `Backend` from it.
- `make_windows_credential_manager_vault_with_backend` takes an optional second
  backend. A vault built without one answers every refresh operation
  `unavailable` — that is the one thing `unavailable` now means here, an
  unreachable store, exactly as for the base slot. Both native factories build
  both backends, so a shipped Windows vault always has its second slot.

### The negative control

The new cases were run once against the stub before the change. The stub was
reproduced exactly — `execute` routing the refresh slot to a null backend,
which is what the three stubs did — so the cases compile and fail for the
reason the card names rather than failing to build:

```text
1/4 Test #185: windows_credential_manager_contract .........***Failed  0.04 sec
FAILED: the refresh store succeeds against its own backend
2/4 Test #186: windows_credential_manager_native_setup .....***Failed  0.02 sec
3/4 Test #187: windows_credential_manager_native ...........***Not Run 0.00 sec
4/4 Test #188: windows_credential_manager_native_cleanup ...***Failed  0.01 sec
0% tests passed, 4 tests failed out of 4
```

The native rows fail at setup because `ensure_native_item_absent` now clears
the refresh target too, and against the stub that clear answers `unavailable`.

### Verification

`ctest --preset windows-dev -R '^(windows_credential_manager_contract|windows_credential_manager_native_setup|windows_credential_manager_native|windows_credential_manager_native_cleanup|application_github_credential_vault|application_device_authorization_refresh|application_device_authorization_refresh_course)$' --output-on-failure`,
confirmed with `-N` first (`Total Tests: 7`):

| Selector | Result | Time |
|---|---|---|
| `application_github_credential_vault` | Passed | 0.04 s |
| `application_device_authorization_refresh` | Passed | 0.03 s |
| `windows_credential_manager_contract` | Passed | 0.04 s |
| `windows_credential_manager_native_setup` | Passed | 0.02 s |
| `windows_credential_manager_native` | Passed | 0.07 s |
| `windows_credential_manager_native_cleanup` | Passed | 0.01 s |
| `application_device_authorization_refresh_course` | **flaky at this head, not this card's** | 6.7–6.9 s |

Six of the seven are this card's and all six are green. The seventh is
recorded below rather than claimed.

```text
cmake --build --preset windows-dev --config Debug          exit 0
clang-format --dry-run --Werror <the four touched files>   exit 0
py -3 scripts/verify_documentation.py --root .             DOCUMENTATION_OK, exit 0
git diff --check                                           exit 0
```

No `C4456`, `C4189` or `C4996` appeared; the nested locals this slice adds are
distinctly named.

**Strict tidy could not be made to pass, and did not regress.** The
`windows-dev` tree is a Visual Studio generator and emits no
`compile_commands.json`, so `tidy.py --build-dir build/windows-dev` exits 2
with `compile_commands.json not found`. Running it against the Ninja
`windows-app-release` tree instead reaches
`src/adapters/github/src/windows_credential_manager.cpp` (the two test files
are not in a tests-disabled tree, so they cannot be tidied on this host at
all):

```text
py -3 _coding-standard/scripts/tidy.py --build-dir build/windows-app-release
  --warnings-as-errors --paths src/adapters/github/src/windows_credential_manager.cpp
  before this change   78 warnings treated as errors
  after  this change   78 warnings treated as errors
```

All 78 are `readability-identifier-naming` on the file's pre-existing
`snake_case` parameters, and they fail at the merged head with none of this
card's changes present. The first draft of this slice added four more of them
(`refresh_backend` on the two constructors and the two factories); those are
renamed `aRefreshBackend`, which returns the file to its baseline exactly.
Bringing the file to zero means renaming ~78 pre-existing parameters, which is
neither this card's scope nor a change any test would notice.

### `application_device_authorization_refresh_course` is flaky at this head

It failed in the first run of the set here, and the station spent the time to
place it rather than claim it. It is **not** this card's: the course runs over
`fixture_credential_vault` and never constructs a Credential Manager vault at
all (`github_application.cpp` builds the platform vault only when
`Fixture == nullptr`).

Measured at pristine `8f9d5616`, with all four of this card's files reverted:

```text
one run immediately after a build          Passed  6.69 s
the next run                               Failed
eight consecutive runs after that          passed=0 failed=8
```

The failure is always the same single marker — `DEVICE_FLOW_VAULT_STORES:1`
against the required `2`, so `DEVICE_FLOW_REFRESH_FAILED` — with
`STAGE:authorized`, `REQUESTS:0`, `ROTATIONS:1`, `REFRESH_STATE:scheduled` and
`REFRESH_BOUND:true` all correct. It passes once after a relink and then
fails, which points at the rotation's second vault store not landing inside
`bpr_self_test_budget_ms` rather than at a race. The fixture's application
data root is a fresh `QTemporaryDir` per process, and the six stale
`bpr-public-path-*` directories in `%TEMP%` are all from August, so leaked
run state is not the cause.

With this card's change in place the same selector passed in two of the four
sets run here and failed in two, at the same rate as pristine. Recorded for
whoever owns [[BPR-0536]]'s course; nothing here repairs it.

### Claims

| Claim | Command | Result |
|---|---|---|
| Refresh material lands in a second slot of the same store, keyed by the same authority, under a distinct target | `windows_credential_manager_contract` | Passed; `test_the_refresh_slot_reaches_only_its_own_target` proves the credential backend never sees the refresh material and a credential query is not answered by the refresh target |
| The slot works against the real Credential Manager | `windows_credential_manager_native` | Passed; store, retrieve under the authority, absent from the base slot, a base credential beside it unaffected, gone after `delete_refresh` |
| An absent refresh entry deletes successfully rather than answering unavailable | `windows_credential_manager_native` | Passed; `run_native_refresh_lifecycle` requires the `absent` disposition, which is what [[BPR-0540]]'s orderly-quit erasure relies on |
| `unavailable` means only an unreachable store | `windows_credential_manager_contract` | Passed; `test_a_vault_without_a_refresh_target_reports_unavailable` is the only case that still expects it |
| Each new case fails against the stub | the negative control above | 4 of 4 failed, the contract on `the refresh store succeeds against its own backend` |
| Format, documentation and whitespace gates | `clang-format --dry-run --Werror`, `verify_documentation.py`, `git diff --check` | exit 0, `DOCUMENTATION_OK`, exit 0 |

Nothing outside the vault, its two test files and this card was touched: no
device host, no composition, no macOS or Linux, no `scripts/`, no other
documentation.
