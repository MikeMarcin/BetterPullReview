---
schema: "implement-rfc.card.v1"
id: "BPR-1117"
title: "The Linux Secret Service rows use whatever session bus the shell names, including the user's real keyring, and nothing refuses a non-designated bus"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"BPR-1113 finding 5, documented at d03e2d61 from BPR-0906's independent review","related_cards":["BPR-1113","BPR-0906","BPR-0310","BPR-0024","BPR-0029"],"reservation":"docs/issues/_reservations/BPR-1117--BPR-1118--7bb2c9ebce22.json"}
---

# BPR-1117: The Linux Secret Service rows use whatever session bus the shell names, including the user's real keyring, and nothing refuses a non-designated bus

## Why

The three native rows, `linux_secret_service_native_setup`,
`linux_secret_service_native` and `linux_secret_service_native_cleanup`, run
the production Secret Service vault against whatever session bus the
environment names. They write, replace and delete their integration item in
the collection the `default` alias names.

- With `DBUS_SESSION_BUS_ADDRESS` set, that is the bus it names. In a desktop
  shell, that is the user's own session bus and login keyring.
- With it unset, the Linux launcher pins it to `$XDG_RUNTIME_DIR/bus` before
  it gives the row its own runtime directory ([[BPR-0906]]). On a desktop
  that is again the user's own bus.

[[BPR-1113]] recorded this as its finding 5, in
`docs/development/build-and-test.md`. It ran the rows by hand on a private
`dbus-daemon` with a throwaway keyring. Nothing makes that the only way:
nothing in the rows refuses a bus, and `docs/development/remote-stations.md`
tells a Linux station to unlock its own login collection so that these rows
pass.

The controlled direct checks in the same executable have the same hole.
`--expect-locked` and `--expect-bus-redirect-rejected` use whatever bus is
named. `--expect-unavailable` checks only that no address is set, so a
runtime directory holding the user's `bus` socket still reaches it.

## Scope

- **Designation.** The rows run only on a session bus that the test harness
  started, or that an operator explicitly designated. Anything else is a
  named refusal, with a failing exit, before any Secret Service call.
- **One daemon, not one path.** The designation names one daemon instance,
  so a stale designation, or a path another daemon now serves, is refused
  as well.
- **A harness-started private bus.** A harness starts a private
  `dbus-daemon` and a test-only `gnome-keyring-daemon`:
  - the bus has no service directories, so it activates nothing;
  - the keyring is unlocked from standard input with a random password, under
    fresh directories;
  - the harness designates that bus, runs the row, and stops both daemons by
    exact PID.

  The three rows are registered through it. `ctest` then runs them in CI and
  on stations without a desktop session, and never on the user's keyring.
- **The direct checks.** `--expect-locked` and
  `--expect-bus-redirect-rejected` need the designation too.
  `--expect-unavailable` refuses while any bus is named: by address, by a
  runtime directory, or by a display that autolaunch could use.
- **Pin it.** A registered row pins the refusals.
- **Documentation.** `docs/development/build-and-test.md` covers the
  launcher's bus pin and the Secret Service section.
  `docs/development/remote-stations.md` covers the login-collection unlock
  these rows no longer need.

## Acceptance

- AC1 Every mode of the native test except `--expect-unavailable` refuses
  with a named line and a failing exit, before any Secret Service call, when:
  - no bus is designated;
  - the bus named is not the designated one, whether by address or through
    the runtime-directory fallback the launcher pins;
  - the designation is malformed;
  - the designated address is served by a daemon other than the designated
    one.

  `--expect-unavailable` refuses while any bus is named.
- AC2 A negative control points the rows at a second private `dbus-daemon`
  started for the control, with its own test-only keyring. The rows refuse,
  and that keyring is unchanged. In a reverted trial without the check, the
  same run proceeds on that bus, which shows the refusal is what stops it.
- AC3 With no bus address in the environment and no desktop session,
  `ctest -R '^linux_secret_service_'` passes. Each native row runs on its
  own harness-started bus and keyring. Both daemons are stopped by exact PID,
  and neither a process nor a `/tmp` root is left behind.
- AC4 A registered row pins the refusals, and a trial without the check
  fails it.
- AC5 No regression:
  - `linux_secret_service_contract`;
  - `test_network_isolation_manifest`, because registrations change;
  - `msvc_secure_crt_contract`;
  - both `contained_temporary_*` rows;
  - both architecture boundary rows;
  - `verify_documentation.py`.
- AC6 The documentation above says what the rows need and how they are kept
  off the user's bus.

Linux only: the native test, its rows and the harness exist only on Linux.

## Verification safety

- Every run is offscreen, with `DISPLAY` and `WAYLAND_DISPLAY` unset.
- `DBUS_SESSION_BUS_ADDRESS` is unset, or overridden to a private daemon the
  run started.
- The user's session bus, its keyring and `/run/user/<uid>` are never
  addressed.
- Every daemon a run starts is stopped by exact PID.

## Fixed at `8cfba187` — bpr-main-linux, 2026-09-24

Claimed at `f910b7e3`. The product is untouched. The change is one commit,
`8cfba187`:

- the native test, `tests/github_contract/linux_secret_service_native_tests.cpp`;
- a new harness, `tests/scripts/linux_secret_service_private_session.py`;
- the registrations in `tests/github_contract/CMakeLists.txt`;
- `docs/development/build-and-test.md` and
  `docs/development/remote-stations.md`.

It was verified before its rebase. The rebase brought in only documentation.

### The designation

Every mode of the native executable except `--expect-unavailable` checks the
bus first. That includes the three rows, the audit's own `--native-child`,
`--expect-locked` and `--expect-bus-redirect-rejected`. The check refuses
with `linux-secret-service-bus:refused:<reason>` and exit 1, before the vault
is made, unless all of these hold:

1. `BPR_DESIGNATED_TEST_SESSION_BUS` is set. Otherwise: `undesignated`.
2. It is exactly one `unix:path=<absolute path>,guid=<32 lowercase hex>`
   address, the form `dbus-daemon --print-address` writes for a path socket.
   Otherwise: `designation-malformed`. So a copied `DBUS_SESSION_BUS_ADDRESS`
   such as `unix:path=/run/user/1000/bus`, which has no GUID, cannot
   designate anything.
3. `DBUS_SESSION_BUS_ADDRESS` is exactly that address. Otherwise:
   `not-designated`. A bus found through the runtime directory, which is what
   the launcher's pin produces, has no such address.
4. The connection GIO hands out for the session bus reaches a server that
   reports that GUID. Otherwise: `bus-unreachable` or
   `bus-identity-mismatch`. The connection stays referenced, so the vault's
   proxies are built on the connection that was checked.

The first three checks make no connection at all.

A GUID names one daemon instance: each `dbus-daemon` start makes a new one.
It is the server GUID from authentication, and it is not the same as
`org.freedesktop.DBus.GetId`. This was measured: a private daemon printed
`guid=17a82a4c…` and answered `GetId` with `bd798183…`.

The audit's child gets the designation in its fixed environment and checks
for itself.

`--expect-unavailable` must find no bus at all. It refuses with
`session-bus-named` while `DBUS_SESSION_BUS_ADDRESS`, `XDG_RUNTIME_DIR` or
`DISPLAY` is set, whatever the value. Before, it checked only the address.

### The harness

`tests/scripts/linux_secret_service_private_session.py`:

1. makes an owner-private root (`bpr-secret-service-*`) with the contained
   temporary helper;
2. starts `dbus-daemon --nofork` on a path socket there. Its configuration
   has no `<servicedir>`, so it activates nothing. The address comes from
   `--print-address` on a pipe;
3. starts `gnome-keyring-daemon --foreground --unlock --components=secrets`
   on that bus, under fresh directories. Its random password is written only
   to the daemon's standard input;
4. waits, bounded, until `default` names an unlocked collection;
5. runs the row with only `PATH`, `LANG`, fresh `HOME`, the XDG directories,
   `TMPDIR`, the boundary marker, and the bus designated;
6. stops the keyring and then the bus through their own `Popen` handles, and
   refuses to report clean if the bus had any child;
7. removes the root.

Each daemon and the row carry `PR_SET_PDEATHSIG`. SIGTERM and SIGHUP unwind
the harness through its cleanup.

The three rows are registered through the harness, and a fourth row,
`linux_secret_service_refusal_control`, pins the refusals. It starts a
designated bus and a second one, with no keyring on either. It then requires
the exact refusal line and a failing exit for seven cases:

- no designation;
- the other bus by address;
- the other bus through a runtime directory;
- a designation without its GUID;
- the designated path with the other server's GUID;
- the native row on the other bus;
- `--expect-unavailable` with a runtime directory holding a bus.

It also requires the designated bus to be let through to the vault, which
finds no Secret Service there.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` (Debug), dbus 1.16.2,
gnome-keyring 50.0. Every run was offscreen, with `DISPLAY`,
`WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset. The CTest runs used
fresh absolute XDG roots, through a wrapper that recorded, by a read-only
listing, any `bpr-` name a run added to `/run/user/1000`. None added one.

Only daemons these runs started were addressed. The user's own `dbus-daemon`
and `gnome-keyring-daemon` processes were listed before and after, and were
never signalled or connected to.

| Claim | Check | Result |
| --- | --- | --- |
| AC1 | `linux_secret_service_refusal_control`, and the negative control below | Each case prints its refusal: `undesignated`, `not-designated` by address, by runtime directory and for the native row, `designation-malformed`, `bus-identity-mismatch`, and `session-bus-named` for `--expect-unavailable`. The designated bus reaches the vault |
| AC2, the rows refuse a second bus | `negative-control-candidate.log`. B is a second private `dbus-daemon`, pid 1016349, with its own test-only keyring, pid 1016350. A is a third daemon, designated, with no keyring. `--fixture-setup`, the audit and `--fixture-cleanup` each ran under the network sandbox launcher with B's address | All six runs exit 1 before any vault call: `refused:undesignated` without a designation, and `refused:not-designated` with A designated. B's collection has no items before or after, and the SHA-256 of its `login.keyring` and `user.keystore` is unchanged |
| AC2, the refusal is what stops them | The same control with `HEAD`'s executable, which has no check (`negative-control-trial-no-check.log`) | All six runs pass on B: `setup:pass`, `audit:pass` (store, retrieve, replace and delete on B's keyring) and `cleanup:pass`. B's `login.keyring` digest changes. On a desktop, B would have been the user's keyring |
| AC3 | `ctest -R '^linux_secret_service_'`, `-j1` (`candidate-rows.log`), and again `--repeat until-fail:5` (`final-rows-repeat5.log`) | 5/5 both times. Each native row prints `private-session:started:bus-pid=…:keyring-pid=…`, then its own `pass` line, then `private-session:stopped`. No `/tmp/bpr-secret-service-*` or `/tmp/bpr-run-*` is left |
| AC3, signals | The harness around `/bin/sleep 30` (`harness-signal-trials.log`) | SIGTERM: exit 143. The bus, the keyring and the row are gone, and the harness removed its root. SIGKILL: exit 137. All three are gone through `PR_SET_PDEATHSIG`, and the root is left. That leftover was removed by exact name |
| AC4 | `linux_secret_service_refusal_control` with `HEAD`'s executable (`trial-no-check-control-row.log`) | The row fails, and all seven cases are reported. `HEAD`'s `--expect-unavailable`, with only a runtime directory naming a live bus, printed `linux-secret-service-unavailable:pass`: it had reached that bus |
| AC5 | `integration.log`, `-j4` | 14/14: the five `linux_secret_service_*` rows, `test_network_isolation_manifest` (registrations changed), `msvc_secure_crt_contract`, both `contained_temporary_*` rows, `architecture_boundaries`, `architecture_boundary_contract` (186 s), `documentation_structure`, `documentation_issue_graph_contract` and `sqlite_profile_activation` |
| AC6 | `verify_documentation.py` | `DOCUMENTATION_OK` |
| Gates | `/usr/bin/clang-format` 22, `--dry-run -Werror`; scoped `tidy.py --warnings-as-errors` on the native test; `git diff --check` | Pass |

After the `HEAD` trials, the candidate file was restored, compared byte for
byte with the saved copy and its diff, and rebuilt. The logs are station
records under `build/logs/bpr-1117/` and `build/logs/bpr-1117-1118/`, and are
not committed.

### Limits

- **An operator can still designate a real bus.** Explicit designation needs
  that bus's exact address and GUID. The documentation says never to
  designate a bus whose keyring holds anything real. The check stops an
  accident, not someone who means it.
- **A harness killed outright leaves its `/tmp` root.** The daemons still
  die with it. The root holds only its own test-only keyring and is removed
  by name. As in [[BPR-1113]], no sweep is added.
- **The fixture rows lost their cross-run role.** Each row has its own
  keyring, so the setup and cleanup rows can no longer find an item another
  run left. On the old shared keyring that was their purpose. The native
  row's exact post-test absence check still proves its own cleanup.
- **The rows need the tools.** `dbus-daemon`, `dbus-send` and
  `gnome-keyring-daemon` must be in `/usr/bin` or `/bin`. Without them each
  row fails with `private-session:unavailable:<tool>-missing`, as it failed
  before without a Secret Service.

### Evidence owed elsewhere

None. The native test, the harness and all four rows exist only on Linux.
The CMake change sits inside the Linux block.
