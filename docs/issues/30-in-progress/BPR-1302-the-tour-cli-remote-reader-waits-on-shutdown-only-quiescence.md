---
schema: "implement-rfc.card.v1"
id: "BPR-1302"
title: "The tour CLI's live remote reader waits on shutdown-only host quiescence, so online open and connect never return"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1068's Linux run, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1302--BPR-1302--bb2b61bce2e0.json","related_rfcs":["RFC-BPR-0044"],"related_cards":["BPR-1068","BPR-1055","BPR-1059","BPR-1222"],"blocks":"AC44-12 (first-run auth setup) and every live use of bpr tour open, refresh, publish and recover"}
---

# BPR-1302: The tour CLI's live remote reader waits on shutdown-only host quiescence, so online open and connect never return

Found by bpr-main-linux on 2026-10-06 during [[BPR-1068]], at `112665f81`. That commit
was published as `3632c4035`; the line numbers below are unchanged by the rename.

## Observation

`bpr tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json` on a
fresh profile that has never signed in, from the installed TourCLI tree
(Linux, WSL2, GCC 15.2, `tour-cli` preset, no display, no session bus),
printed nothing and did not exit for over 120 seconds. SIGINT ended it with
exit 130. It should have refused `authentication_required` (exit 5) at once.

A temporary instrumented build (not committed) showed where:

```
DBG restore start
DBG restored 1            # device_profile_restore_status::unavailable: no binding
DBG waiting restored=1 quiescent=0 effects_idle=1   # repeated until SIGINT
```

`remote_reader::open` in `src/app/src/tour_remote.cpp` waits for
`restored && Host->is_quiescent()`. `github_application_host::is_quiescent()`
begins `return Implementation->Closing && ...`: it is the "safe to destroy
after shutdown" predicate, and `Closing` is set only by `shutdown_now()`. So
while the host runs it is always false, and the wait ends only when SIGINT
shuts the host down. `epoch_application_host::is_quiescent()` has the same
`Closing` guard.

## Affected waits (all in `tour_remote.cpp`)

| Line at `112665f81` | Path | Effect |
|---|---|---|
| 94 | `connect --transport gh`, after `activate_host_profile_intent` | never returns (macOS, the only platform where gh is composed) |
| 120, 122 | `open` native: restore, then settle | never returns; observed on Linux |
| 127 | `open` (both transports), after `open_pull_request_intent` | never returns |
| 203 | first pinned-source read, after `Epoch->accept` | never returns |
| 327 | device `connect`, after the flow | inferred: would not return after a person authorized, although the grant and binding would already be stored |

Line 344, in `stop()` after `shutdown()`, is the one correct use.

The fixture courses (`tour_cli_composition`, `tour_cli_publication`,
`tour_cli_terminal_publication`) inject a reader factory and never construct
`remote_reader`, and no ledger records a live `bpr tour open` or `connect`
on any platform, so nothing has exercised these waits.

## Scope

- Wait on what each step is for: the restore callback; the session leaving
  `PendingSession` (accepted or failed); the pull request leaving
  `PendingPullRequest` with a snapshot or a failure; the epoch handoff
  accepted. If the host has no running-idle predicate, add one beside
  `is_quiescent()` rather than reusing the shutdown one.
- A course that constructs the real `remote_reader` against a loopback
  provider (the existing native HTTPS test seam, absent with
  `BUILD_TESTING=OFF`): a never-signed-in `open` refuses
  `authentication_required` promptly; a fixture session and pull request
  open and return; and the same course with the old waits hangs.
- No live GitHub is needed to fix it. Afterwards, live qualification uses
  only `MikeMarcin/BPR-Test`.

## Acceptance

- `bpr tour open URL --json` on a never-signed-in profile exits 5
  `authentication_required` within seconds, on Linux, macOS and Windows.
- The loopback course passes, and fails with the old waits restored.
- [[BPR-1068]]'s device-flow completion on Linux (owner enters the code)
  then returns `Connected account …`, and the following `open` of a
  `MikeMarcin/BPR-Test` pull request returns its draft.

## Implementation (bpr-main-linux, 2026-10-06)

Ubuntu 26.04 under WSL2, GCC 15.2.0, CMake 4.4.2, Qt 6.11.2, OpenSSL 3.5.5,
git 2.53.0, gnome-keyring 50.0, dbus-daemon 1.16.2. Start `f97cb0f2d`;
product, course and tests published as `af4fa4fd8`. The evidence below was
produced on the same change as `f6b513762`, before `git pull --rebase` put it
on `3754fee9b` (BPR-1293); at the rebased head the `tour_cli_*` rows, the
composition contract, `architecture_boundaries`,
`test_network_isolation_manifest`, `application_github_host`,
`application_epoch_host`, `cycle_cli_public_path` and
`sqlite_tour_owner_channel` pass again, 13 of 13.

### Root cause

Three defects, the first one hiding the other two.

1. Every step of `remote_reader` waited on `is_quiescent()`, which on the
   GitHub and epoch hosts begins with `Closing` and is true only after
   `shutdown()`. So online `open`, `refresh`, `publish`, `recover`, the first
   pinned-source read, gh `connect` and the end of native `connect` waited
   until SIGINT. A device flow that failed early (no network) looked like the
   same hang: the public-entry course expected exit 130 for it.
2. A completed native sign-in was never stored for the next process. The
   desktop binds the device host's renewal material to the account `/user`
   names (`bind_device_refresh_authority`, called from its projection), and
   only then does the GitHub host remember the device binding. The terminal
   composition had no such step, so the grant was used once and forgotten and
   the following `open` answered `authentication_required`.
3. Binding late, after the session's turn, is not enough either: the host's
   renewability probe and the device host's refresh store use the same vault
   operation context (request 1, same session, authority and generation), and
   the vault refuses the second one while the first is pending. The desktop
   binds inside the accepted session's publication, before the probe is
   queued, so its store wins; binding after the turn lost every time
   (`storage_failed`), and won only when a debug print delayed it.

### Fix

- `github_application_host::is_running_idle()` and
  `epoch_application_host::is_running_idle()`: true while the host runs and
  owes nothing, false once closing. Each host's settled-work terms now live in
  one private `outstanding_work_settled()`, which `is_quiescent()` (unchanged:
  `Closing &&` those terms) and `is_running_idle()` (`!Closing &&` those terms;
  the GitHub host also requires no turn or deferred ingress) both read. The
  renewability probe is not counted: it may never answer, and nothing a read
  waits for depends on it.
- `tour_remote.cpp` waits on `is_running_idle()` after each intent, and
  `is_quiescent()` only in `stop()` and on cancellation. The wait blocks on
  the decision lane (`terminal_decisions::wait_for_post`), so it wakes on the
  next host completion instead of sleeping 1 ms; the 10 ms slice serves SIGINT
  and the device poll's Qt timer.
- `remote_reader::publish` binds the renewal authority when the accepted
  session is published, as the desktop does. Native `connect` ends when the
  renewal is scheduled and the binding is durable, refuses `vault_unavailable`
  if GitHub authorized a sign-in that could not be stored, and names a flow
  that ended unauthorized (`authentication_denied`, or
  `authentication_unavailable` with "could not reach GitHub or did not
  complete").
- Test-enabled builds only: `BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK=PORT:PEM`
  routes the reader's provider and device transports through the GitHub
  adapter's existing loopback proxy and fixture-CA factories
  (`BPR_NATIVE_HTTPS_TESTING` on the tour runtime, as BPR-0515 gives the
  desktop composition). The tests-off `tour-cli` binary contains neither the
  variable nor a `_for_test` symbol (`strings`: 0 matches).

### Loopback provider course

`tour_cli_loopback_provider` (`tests/scripts/tour_cli_loopback_tests.py`,
`LOOPBACK_PORT_COUNT 1`, pinned in `MACOS_LOOPBACK_AUTHORITIES`) runs the real
`bpr` through the real reader, provider, native HTTPS transport, device host,
Secret Service vault, epoch host and Git cache. One Python listener at
127.0.0.1 is the CONNECT proxy and the TLS origin for github.com and
api.github.com under a disposable authority it makes with `openssl`. It serves
the device flow, `/user`, the nine capability parts and the github-com
contract fixture's pull request read (routes from
`rfc0015_cli_read_fixture.prepare`), and Git smart HTTP v2 from a real
`git upload-pack` over the cli-read-v1 pack; it records every exchange and
classifies any non-GET REST call or GraphQL mutation as a write. On Linux the
vault is a private bus and test keyring started with
`linux_secret_service_private_session`'s `PrivateBus`/`start_keyring`. Each
step is one `bpr` process bounded at 30 s; a step that does not return is
interrupted and reported `timeout` with the requests the fake saw, and the
course continues. `--seed-binary` lets a good build prepare the profile so a
reverted build reaches every path. macOS adds gh `connect` and `open` with the
fake `gh` on a temporary `PATH` when run with `--gh` outside ctest (inside it
gh's own `sandbox-exec` cannot start); native rows run on Linux only, because
elsewhere the native vault is the user's own.

### Result per path (Linux, ctest, inside the network sandbox, 18.5 s)

| Path | Result |
|---|---|
| Never-signed-in `open` | exit 5 `authentication_required`, no request, 0.0 s |
| Native `connect` (fake device flow, pty) | `Connected account 1001`, 7.0 s (one 1 s poll); the user code is shown, no token or device code reaches the terminal; refresh stored, binding durable |
| Online `open` | exit 0, `status: ok`, the full read, 1.9 s |
| First pinned-source read (`add review.cpp:1-2`) | exit 0, `git:discovery`, `git:ls-refs`, `git:fetch`, 2.4 s |
| `show stop:1 --source` | `available`, exact bytes `int review_line_1 = 21;` / `int review_line_2 = 22;`, 2.0 s |
| Refresh (`preview --refresh`) | exit 0 after a fresh read; status "operation qualification is still required", 1.8 s |
| Publish (pty, complete and ready draft) | exit 5 at the unqualified gate after the read, no prompt, no write reached the fake, 1.9 s |
| Recover | exit 0 after a fresh read, `publication_state: unavailable`, 1.9 s |
| gh `connect`, gh `open` | Not run (macOS only; owed below) |

The tests-off `tour-cli` build, in the network sandbox on a fresh root:
`bpr tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json` exits 5
`authentication_required` in 0.01 s.

### Failing-before controls (20 s step bound, then restored)

| Control | Course result |
|---|---|
| Every `Host->is_running_idle()` wait back to `Host->is_quiescent()`, seed = fixed build | all eight paths `timeout` at 20.0 s, each after the requests it had reached (the session read, then nothing) |
| Only the epoch wait back to `Epoch->is_quiescent()` | `first-pinned-source-read` and `pinned-source-show` `timeout`; the other six pass |
| The publish-time refresh binding and the durable check removed (no seed) | `native-connect` passes but `online-open` fails `authentication_required` with no request; the rest blocked |
| `is_running_idle()` implemented as `Closing && …` | `application_github_host` fails "a running host that owes nothing is idle and not quiescent" |

The rebuilt fixed binary was byte-identical to the one saved before the
controls.

### Tests

`build/dev-tour-cli` (`cmake --preset dev -B build/dev-tour-cli
-DBPR_BUILD_TOUR_CLI=ON`), 22 of 22: `tour_cli_publication`,
`tour_cli_terminal_publication`, `tour_cli_composition`,
`tour_cli_process_commands`, `tour_cli_public_entry`,
`tour_cli_loopback_provider`, `cycle_cli_public_path`,
`sqlite_tour_owner_channel`, the five `linux_secret_service_*` rows,
`architecture_boundaries`, `architecture_boundary_contract`,
`test_network_isolation_manifest`,
`application_review_round_trip_composition_contract`, `application_github_host`
(with the new running-idleness case), `application_epoch_host`,
`application_epoch_contract`, `application_device_authorization_host`,
`application_device_authorization_refresh`. Also 20 of 20 for
`skill_pr_tour_package`, `cycle_cli_documentation`, `application_github.*` and
`application_epoch.*`. `tour_cli_public_entry`'s unlocked-keyring case now
expects the flow's own exit 5 refusal at once; it encoded the hang as exit 130.
`coding-format-check` (clang-format 22.1.8) and the documentation gate pass.
The optimized tests-on tree (`cmake --preset app-release -B
build/app-release-tests -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON`, GCC 15
`-O3 -Werror`) builds all 2244 steps with no warning outside the vendored
`sqlite3.c`; the course passes against its `bpr` too. The tree was deleted.

### Pin

No pinned digest drifted (`full_application` `eaf49c94…`,
`production_app_manifest` `3d8e96ea…`, `production_mutation_branch`
`e067befe…`): no file in the production manifest changed, and no `src/app`
source was added. No reseal.

### Limits

- macOS and Windows are not run. The gh rows have never run; whether the fake
  `gh` passes the macOS configuration checks is unknown.
- Live publication stays behind the unqualified host/transport gate, so the
  course proves publish reaches and returns from that gate; no write path is
  exercised.
- The terminal composes no renewal: after the access lifetime, by the host's
  restore rule, `open` answers `authentication_required` until `connect` runs
  again. Not exercised.
- The probe/refresh-store vault context collision (cause 3) is avoided by
  ordering, as on the desktop; the shared context itself is unchanged.
- No live GitHub was used.

### BPR-1068 re-run

The Linux observation BPR-1068 could not take can now be run without anything
but the owner's code entry: `bpr tour connect --host github.com --transport
native` in a session with an unlocked keyring, the owner enters the printed
code at `https://github.com/login/device`, and the following `open` of a
`MikeMarcin/BPR-Test` pull request should return its draft.

### Owed station runs

macOS (Terminal):

```sh
cd /absolute/path/to/BetterPullReview && git pull --rebase --autostash
cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build build/dev-tour-cli
ctest --test-dir "$PWD/build/dev-tour-cli" --output-on-failure -R "^(tour_cli_loopback_provider|tour_cli_public_entry|tour_cli_composition|tour_cli_publication|tour_cli_terminal_publication|tour_cli_process_commands|application_github_host|application_epoch_host|test_network_isolation_manifest|architecture_boundaries)$"
openssl=$(sed -n 's/^BPR_TOUR_LOOPBACK_OPENSSL:FILEPATH=//p' "$PWD/build/dev-tour-cli/CMakeCache.txt")
python3 "$PWD/tests/scripts/tour_cli_loopback_tests.py" "$PWD/build/dev-tour-cli/src/app/bpr" --openssl "$openssl" --gh
```

Windows (PowerShell in a native x64 tools environment):

```powershell
Set-Location C:\absolute\path\to\BetterPullReview; git pull --rebase --autostash
cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build build/windows-dev-tour-cli --config Debug
ctest --test-dir "$PWD\build\windows-dev-tour-cli" -C Debug --output-on-failure -R "^(tour_cli_loopback_provider|tour_cli_public_entry|tour_cli_composition|tour_cli_publication|tour_cli_process_commands|application_github_host|application_epoch_host|test_network_isolation_manifest|architecture_boundaries)$"
```

Record each result, the revision and the OS, Qt and compiler here. On both,
`tour_cli_loopback_provider` runs the never-signed-in `open` (exit 5 within
its 30 s bound, elapsed printed); on macOS the `--gh` run adds gh `connect`
and `open`.
