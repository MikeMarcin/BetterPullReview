---
id: BPR-0311
title: Verify device authorization on target-native Windows x64
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0036, BPR-0282]
---

# Verify device authorization on target-native Windows x64

## Required final state

- A real supported Windows 10 or 11 x64 host builds the registered tests-
  disabled Release path and runs native HTTPS, Credential Manager, host polling/
  refresh, Qt presentation, zero-request, dependency, PE/runtime, and secret-
  scan evidence.
- System trust/proxy, system-browser handoff, cancellation/shutdown, vault
  storage/rotation, and Release selection are target-native; macOS, Wine,
  cross-compilation, mocks, or hosted workflows do not substitute.
- Device capability stays unavailable on Windows until this card is terminal.
- After all target-native rows pass, this card owns the Windows production-
  selection flip in the same commit and reruns the complete Release/public/
  native/hygiene matrix against the enabled artifact before becoming terminal.
- BPR-0311 adds and records a native `windows-app-release` tests-disabled
  Release preset before the flip. The one-time flip record is distinct from
  BPR-0285's idempotent final-head platform course against enabled Windows.
