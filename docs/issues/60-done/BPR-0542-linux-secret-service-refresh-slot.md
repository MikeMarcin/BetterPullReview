---
id: BPR-0542
title: The Linux Secret Service vault stores refresh material in a real second slot
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The Linux Secret Service vault stores refresh material in a real second slot

## Why

The RFC-BPR-0016 closeout review of the platform vaults on 2026-09-03
([[BPR-0285]], the integrator's own reading before the third independent
review) found that `src/adapters/github/src/linux_secret_service.cpp` implements the refresh slot as three
stubs: `store_refresh`, `retrieve_refresh`, and `delete_refresh` each answer
`github_credential_vault_failure::unavailable` without touching the
platform store. The recorded distributable registration issues
expiring user tokens with refresh tokens
(`registration-observation.json`: `user_access_token_expiration_enabled:
true`, `refresh_policy: expiring_user_token_with_refresh`), so a real
sign-in on Linux runs this path: the grant parks its refresh token in
memory, the `/user` observation binds the authority and flushes it
(`bind_refresh_authority`), the store answers `unavailable`,
`on_refresh_stored` calls `fail_storage`, and the host reports
`reenrollment_required` with `storage_failed` on the sign-in panel a
moment after the sign-in succeeded. The access token then expires after
its lifetime with nothing to renew it. No course sees this: every enabled
platform course stops at `cancelled` before a grant, and every composed
course runs over the fixture vault, which has a real refresh slot. The
platform table's "Enabled since [[BPR-0310]]" row and the ledger's
refresh-lifecycle claim therefore overstate Linux, and RFC-BPR-0016
cannot close until the slot is real here.

## Required final state

- `src/adapters/github/src/linux_secret_service.cpp` stores, retrieves, and deletes refresh material in a
  second slot of the same platform store, keyed by the same authority as
  the base credential and under a distinct target so one slot can never
  answer for the other — the shape `macos_keychain.cpp` gives its
  `RefreshBackend` and its `github-native-refresh` service. `unavailable`
  is answered only when the platform store itself is unreachable, never as
  a stand-in for an unimplemented slot.
- `linux_secret_service_contract` gains the refresh-slot cases the macOS contract has
  (`test_the_refresh_slot_reaches_only_its_own_service` and the store,
  retrieve, absent, and delete answers), and `linux_secret_service_native` exercises the
  slot against the real Secret Service `login` collection on the station: a stored refresh entry
  is retrievable under its authority, is absent from the base slot, and is
  gone after `delete_refresh`. Each new case fails against the stub before
  the change; record that run as the negative control.
- Erasure honesty: with [[BPR-0540]] deleting the refresh slot beside the
  base credential, an orderly shutdown on Linux leaves no refresh
  material in the store; the native test proves the delete answers success
  for an absent entry rather than `unavailable`.
- The card records the host, toolchain, the exact selectors run, and
  results; format and strict tidy pass on the touched files.

## Non-goals

- Any change to the device host, the composition, the macOS vault, or the
  refresh lifecycle's timing.
- Persisting a session across a restart ([[BPR-0541]]).

## Lane — 2026-09-03

Carded by the integrator straight into `23-ready-lin/` with no open dependency:
the slice is Linux-native (the platform store and its native test)
and is dispatched to `bpr-linux` on `linux-lane`. The integrator merges the
lane, then the RFC-BPR-0016 final-head replays run at the head that
carries it.

## Implemented target-native on bpr-linux — 2026-09-03

The three stubs are gone. `linux_secret_service.cpp` now stores, retrieves and
deletes refresh material in a second Secret Service slot under its own schema,
keyed by the same authority as the base credential, and `unavailable` is
answered only by a composition that has no second store at all.

Host: Ubuntu 26.04 LTS, `Linux 6.18.33.2-microsoft-standard-WSL2 x86_64`.
Toolchain: GCC 15.2.0, CMake 4.4.2, Ninja 1.13.2, Qt 6.10.3, Python 3.13.15,
clang-tidy 22, clang-format 22.1.8. Branch `linux-lane` at main `de172d04`.

The Secret Service `login` collection read `Locked (<false>,)` at
2026-09-03T16:33:23Z, before any work, and again at 2026-09-03T16:37:20Z and
16:42:49Z immediately before each native run. It is recorded because a locked
collection makes the native rows *skip* rather than fail, and a skipped row
reports the same green as a passing one.

### Two schemas, not one schema with two attributes

The refresh slot is a distinct `SecretSchema`
(`org.betterpullreview.github-native-refresh.secret-service.v1`, and its
`…integration-test.v1` sibling), not a second attribute value on the
credential schema. The cheaper design was available and is worse: with one
schema distinguishing the slots by an attribute, a dropped or mistyped
attribute makes one lookup answer for the other, and the failure that produces
is a refresh token handed to a caller that asked for an access token. A query
carries its schema, so with two schemas the credential slot cannot see a
refresh item at all — the confusion is unrepresentable rather than merely
incorrect. This is the shape `macos_keychain.cpp` already had with its
`RefreshBackend` and `github-native-refresh` service.

`v1` rather than `v2` on the new identifiers: they have no history to stay
compatible with, because nothing has ever written an item under them.

The slot is threaded the way macOS threads its own: a `linux_secret_service_slot`
on each queued operation, read once when the operation is dequeued, so the
backend an operation runs against is decided by what the caller asked for and
never by which backend happens to be reachable. The six public port methods are
now three `submit_*` helpers taking a slot, which is also why the store's
validation could not drift apart between the two slots.

### The negative control

Every new native case was run against the stub first, at `de172d04` with no
source change. `linux_secret_service_native` failed, and the categories it
printed name exactly the refresh rows:

```
2/3 Test #190: linux_secret_service_native ...........***Failed    0.40 sec
linux-secret-service-audit:child-lifecycle
linux-secret-service-audit:stdout
linux-secret-service-audit:stderr
linux-secret-service-refresh:initial-absence
linux-secret-service-refresh:store
linux-secret-service-refresh:retrieve
linux-secret-service-refresh:delete
linux-secret-service-refresh:post-delete-absence
linux-secret-service-refresh:repeat-delete
linux-secret-service-refresh:cleanup
linux-secret-service-retrieve:unavailable
linux-secret-service-audit:fail
```

Two things in that output are worth reading rather than skimming.
`linux-secret-service-retrieve:unavailable` is the stub answering, in its own
words, the failure this card exists to remove. And the refresh rows that are
*absent* from the list — `base-store`, `base-unshadowed`, `base-survives`,
`base-cleanup` — passed against the stub, because they exercise the credential
slot. A negative control in which everything fails would not have shown that
the new cases discriminate between the slots; this one does.

The two contract cases have no pre-change run to record and could not have
one: they drive the two-backend injected factory, which did not exist at
`de172d04`. Their negative control is that the third parameter is absent, so
they do not compile — recorded here rather than manufactured by staging a
half-applied change.

### Verification

Every command below was run from `/home/mmarcin/bpr-linux` at this commit.

| Command | Result |
|---|---|
| `ctest --preset dev -N -R '^(linux_secret_service_contract\|linux_secret_service_native_setup\|linux_secret_service_native\|linux_secret_service_native_cleanup\|application_github_credential_vault\|application_device_authorization_refresh\|application_device_authorization_refresh_course)$'` | `Total Tests: 7` — the exact seven, confirmed registered before running |
| the same without `-N`, `--output-on-failure` | exit 0, **7/7 passed** in 6.44 s, every test labelled `bpr_network_isolated` |
| `ctest --preset dev --output-on-failure` (whole matrix, run as extra diligence; not asked for) | 266/267 in 1060 s. The one failure is `conditional_branch_identifiers`, pre-existing at `de172d04` and unrelated to this slice — see below |
| `/usr/bin/clang-format --dry-run --Werror` on the four touched files | exit 0 |
| `python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <the three touched .cpp>` | `clang-tidy passed.`, exit 0 |
| `cmake --build build/dev --target check_boundaries` | exit 0; core boundary and architecture boundaries verified |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK`, exit 0 |
| `git diff --check` | clean |
| `cmake --build build/dev` | exit 0, zero `error:`/`warning:` lines |

**A note on `clang-format` for whoever runs this next on a Linux station.**
The `clang-format` first on `PATH` here is 21.1.8 in `~/.local/bin`, which
shadows the supported 22.1.8 at `/usr/bin/clang-format`. The project's own
`format_check.py` refuses the older tool outright and says why: releases before
22 render an empty braced initializer as `{}` where this tree holds `{ }`, so
21 reports hundreds of files dirty that a supported tool reports clean. Running
the bare `clang-format` the task named would have produced exactly that false
positive — the four touched files show violations at `de172d04` under 21 and
none under 22. The gate above was therefore run with `/usr/bin/clang-format`
explicitly, and the violations it did find were real and are fixed.

### One pre-existing red on main, found by running the whole matrix

Not this slice's, not repaired here, and reported rather than left for the
closeout to trip over. `conditional_branch_identifiers` fails at `de172d04`:

```
src/app/src/system_browser_opener.cpp:91: `address` is used where the declared name is `aAddress`: return installed_launch()(*address);
CONDITIONAL_BRANCH_IDENTIFIERS_FAILED:1
```

`src/app/src/system_browser_opener.cpp` is untouched by this slice and outside
its scope. The line the gate names is a false positive, and the proof is in the
same run: line 91 sits inside `#if defined(BPR_BUILD_TESTING)`, a branch the
`dev` preset *does* compile, and `cmake --build build/dev` compiled it clean
minutes earlier. The local it complains about is real — `auto address =
validated_browser_address(aAddress);` at line 82 — and both arms of the
`#if`/`#else` use it correctly. The gate appears to take the parameter
`aAddress` for the declaration and read `address` as a stale spelling of it,
which its own sibling contract warns is the failure mode to watch: that test
exists because "the gate above reports on source it cannot compile, so nothing
downstream catches it being wrong".

It became red in [[BPR-0285]]'s own tidy-gate commits `5f5b1983` and
`5580e59f`, which changed `const auto address` to `auto address` so the value
could move. Whoever owns it should decide whether the gate's spelling
comparison needs to stop normalizing the project's `a` prefix, or whether the
opener should sidestep it; either way it belongs to that card, not this one.

### What this slice does not do

- The device host, the composition, the macOS vault, and the refresh
  lifecycle's timing are untouched, as the card's non-goals require.
- Persisting a session across a restart stays [[BPR-0541]]'s.
- Windows keeps its stubs; that is [[BPR-0543]]'s, and its `unavailable` still
  means "not implemented" until it lands. [[BPR-0540]]'s correction — treating
  every error from `delete_refresh` as a failure — is safe to make for Linux
  now and is not made here, because it is BPR-0540's own slice and would be
  wrong for Windows until BPR-0543 closes.
