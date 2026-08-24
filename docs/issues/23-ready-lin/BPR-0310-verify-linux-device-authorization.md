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
