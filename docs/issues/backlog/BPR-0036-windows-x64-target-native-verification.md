---
id: BPR-0036
title: Run RFC-BPR-0002 Windows x64 target-native verification
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0034]
---

# Run RFC-BPR-0002 Windows x64 target-native verification

On a designated real Windows 10/11 x64 machine, run the checked-in
`windows-dev` configure, build, and complete CTest matrix. Run the focused
Credential Manager setup/lifecycle/cleanup sequence and prove exact absence
of the integration credential and scratch directory afterward.

Once the dependent slices exist, also run the native HTTPS
TLS/proxy/redirect/certificate matrix, inspect runtime network and process
scope, exercise the public application fixture, and inspect the selected PE
imports and runtime dependencies. Retain only secret-free commands, fixed
output, and inspection evidence.

This card intentionally owns every deferred RFC-BPR-0002 Windows x64
target-native claim. Cross-compilation, mocks, another operating system, and
GitHub Actions are not acceptable substitutes. Its open state does not block
implementation, tests, documentation, security review, or simplification on
the other available targets; it blocks only the affected Windows claims and
RFC-BPR-0002 closeout.
