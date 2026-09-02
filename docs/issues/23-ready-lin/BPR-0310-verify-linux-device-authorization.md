---
id: BPR-0310
title: Verify device authorization on target-native Linux
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0282, BPR-0318]
---

# Verify device authorization on target-native Linux

## Required final state

- A clean supported Linux target builds the registered tests-disabled Release
  path and runs the focused native HTTPS, vault/keyring, host polling/refresh,
  Qt presentation, zero-request, dependency, runtime, and secret-scan matrix.
- System trust, proxy, system-browser handoff, cancellation/shutdown, credential
  storage/rotation, and Release selection are target-native observations rather
  than macOS, cross-compilation, containers without the desktop services, mocks,
  or hosted workflows.
- Device capability stays unavailable on Linux until this card is terminal.
- After all target-native rows pass, this card owns the Linux production-
  selection flip in the same commit and reruns the complete Release/public/
  native/hygiene matrix against the enabled artifact before becoming terminal.
- The one-time flip record and the idempotent final-head course are distinct.
  BPR-0285 reruns the exact platform-course interface against already-enabled
  Linux selection after every required card is terminal.

## Host note — 2026-08-22

The native Secret Service fixtures do not currently pass on the Linux host, so
this card's vault/keyring rows cannot be taken as available without first
resolving that.

Observed while running [[BPR-0443]]. `linux_secret_service_native_setup`,
`_native` and `_native_cleanup` fail; `linux_secret_service_contract` passes.
Proven pre-existing rather than caused by that card's edits: `git stash`,
rebuild, and the same three fail identically at unmodified `HEAD`.

[[BPR-0156]] recorded all four passing on 2026-08-17 against a real
gnome-keyring 50.0, so this is a change in host state rather than a claim that
was never true. `org.freedesktop.secrets` is owned by `gnome-keyring-d` and
`~/.local/share/keyrings/login.keyring` exists, so the service is running; the
likely cause is that nothing unlocked the collection in this session and the
fixture refuses prompts by construction. Not diagnosed further here — it
belongs to whoever runs this card's native course.

## Pre-flip platform course green; the keyring rows named — 2026-09-02

Produced by `bpr-linux` on the JaegAW WSL2 host at `6fff0a53`, read-only,
and recorded here by the integrator. No flip was taken.

**The platform course passes.** `python3
scripts/verify_rfc0016_platform_course.py --platform linux --dev-preset dev
--release-preset app-release --build-root build/rfc0016-linux-enablement
--expected-device-selection unavailable --fresh` exited 0 in 235 s with
`RFC0016_COURSE_SELECTORS:16`, `RFC0016_DEVICE_STAGE:unavailable`,
`RFC0016_DEVICE_REQUESTS:0`, and `RFC0016_PLATFORM_COURSE_OK`. All eleven
stages passed in order; the sixteen selectors were enumerated from the
course's own Debug tree. The station re-derived the surface facts against the
shipped artifact (ELF x86-64 PIE, 10,945,016 bytes): `linux_secret_service`
present (99 symbol-table occurrences), `macos_keychain`,
`windows_credential_manager`, `QWebEngine`, `QWebView`, `QtWebEngine` all
absent. Zero requests from the shipped Release artifact is the "stays
unavailable until terminal" row.

**The Secret Service native rows are blocked by host state, now named.**
`linux_secret_service_contract` passes; `_native_setup` and
`_native_cleanup` fail and `_native` is not run, exactly the 2026-08-22 host
note. Over the session bus, `ReadAlias default` resolves to
`/org/freedesktop/secrets/collection/login`, whose `Locked` property is
`true`; the unlocked `session` collection is gnome-keyring's ephemeral one and
not what the vault targets. The vault resolves `SECRET_COLLECTION_DEFAULT`,
returns `github_credential_vault_failure::locked` when the collection is
locked, and installs a non-prompting service that dismisses every unlock
prompt — deliberate, and what makes the fixture non-interactive. Positive
proof: the native test binary's `--expect-locked` mode passes on the host
right now. Nothing unlocks the collection at login because
`pam_gnome_keyring.so` appears only in `common-password`, and both daemons
were started without `--unlock`; `login.keyring` is dated 2026-08-18, the day
after [[BPR-0156]] recorded the four passing.

**Operator action, not taken by the station.** The `login` collection must be
unlocked in the session the tests run in: either `gnome-keyring-daemon
--unlock` with the session bus address set (per boot), or adding
`pam_gnome_keyring.so` to the `auth` and `session` PAM stacks (durable,
root-owned). Re-aliasing `default` to the in-memory `session` collection
would turn the rows green while invalidating them and is recommended
against. Once unlocked, all four fixtures are expected to pass without a code
change; that is the observation this card's flip waits on.

**For the flip envelope.** The one-shot unlock is per daemon, not per run:
the collection re-locks whenever gnome-keyring restarts, so the flip's rerun
must read `Locked` on `/org/freedesktop/secrets/collection/login` as its
first row, immediately before the course starts, or green rows can quietly
stop being reproducible. The course's `--fresh` rebuilds both trees from
scratch (about four minutes on this host), so a warm tree buys nothing.
