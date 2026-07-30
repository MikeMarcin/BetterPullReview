---
id: BPR-0014
title: Extend the RFC-BPR-0001 runtime audit to Windows
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0011]
---

# Extend the RFC-BPR-0001 runtime audit to Windows

Make the live-process security audit run on Windows as well as macOS and Linux.
Use native PowerShell/CIM and network cmdlets to detect child processes,
listening TCP sockets, and UDP endpoints for the launched application. Keep the
existing secret-canary and teardown checks platform-independent.

## Completion evidence

- The runtime audit remains passing on macOS.
- CTest registers the runtime audit on every supported platform.
- Windows-specific command construction and output parsing have focused tests.
- Actual Windows execution remains a clean native package gate in
  RFC-BPR-0014.

## Result

The audit now uses PowerShell/CIM to find application child processes and
`Get-NetTCPConnection` / `Get-NetUDPEndpoint` to find process-owned Windows
network endpoints. CTest registers the live audit on every supported platform.

## Evidence

- Windows query construction and output parsing: 3/3 focused tests passed.
- Live macOS runtime audit: `RUNTIME_SURFACE_OK`.
- Complete Qt-enabled matrix: 12/12 passed.
- Complete Qt-free matrix: 7/7 passed.
- Documentation validator and `git diff --check`: passed.
