---
schema: "implement-rfc.card.v1"
id: "BPR-1117"
title: "The Linux Secret Service rows use whatever session bus the shell names, including the user's real keyring, and nothing refuses a non-designated bus"
status: "Ready"
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
