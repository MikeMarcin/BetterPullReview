---
id: BPR-0310
title: Verify device authorization on target-native Linux
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0282]
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
