---
id: BPR-0311
title: Verify device authorization on target-native Windows x64
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0036, BPR-0282, BPR-0318]
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

## Deferring — 2026-08-23

The Windows session that cleared the rest of this lane checked this card's
dependencies: they sit in `10-backlog/`, so nothing here can start. The
card stays Ready, deferring, and is not part of the lane's completed set.

## Unblocked — 2026-08-29

All three dependencies are Done: [[BPR-0036]], [[BPR-0282]], and now
[[BPR-0318]], which recorded the distributable identity this card consumes
(public client `Iv23liitiYN1WSSbag50`, admitted complete with the
designated read-only course) and proved the enablement pattern on macOS —
one-time platform-conditional flip in
`shipped_device_authorization_capability()`, real credential-free wire,
probe cancel at `awaiting_authorization`, platform-parametrized selector
expectations, and the release scan permitting exactly the recorded client.
Three unregistered-era gates repaired on the way are recorded on the
BPR-0318 card and matter here too: the Windows enabled course inherits the
truthful `credential_free()` and the merged-header fix. This card starts
the day a native Windows session opens on the `bpr-win-jaegaw` station.
