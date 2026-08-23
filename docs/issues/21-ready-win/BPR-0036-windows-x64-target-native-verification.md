---
id: BPR-0036
title: Run RFC-BPR-0002 Windows x64 target-native verification
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0034, BPR-0347, BPR-0350]
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

GitHub CLI process execution is not part of this card. Milestone one keeps
every CLI selection fixed at zero-launch `cli_unavailable` on all platforms.
If the upstream safety and identity contract ever becomes eligible,
RFC-BPR-0015 owns exact-environment, image, working-directory, Job Object,
descendant, PID, crash-artifact, stream-bound, redirect, configuration, and
account-binding evidence before any Windows CLI claim.

This card intentionally owns every deferred RFC-BPR-0002 Windows x64
target-native claim. Cross-compilation, mocks, another operating system, and
GitHub Actions are not acceptable substitutes. Its open state does not block
implementation, tests, documentation, security review, or simplification on
the other available targets or later RFC work; it blocks only the affected
Windows claims and final RFC-BPR-0002 Auto-Verified closeout.

## Procedure — host policy on a freshly linked image

Recorded from [[BPR-0424]]'s three samples on the designated host, which
runs Smart App Control in its evaluation state
(`VerifiedAndReputablePolicyState=1`). Before any result below is read as a
matrix failure:

- A launch refusal with `win32=4551` (`ERROR_VIRUS_INFECTED`, reported by
  the test as `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551`, by the
  uncontained control as `[WinError 4551]`, and by a nested build as "blocked
  by your organization's Device Guard policy", `MSB8066`) on an image linked
  in the current session is the host policy waiting for a reputation verdict.
  It is per image, not per source, and clears on its own between about
  fifteen and forty-five minutes after the link.
- Confirm it from the Code Integrity operational log: events 3033/3077 name
  the image. Re-run once no new 3077 appears for that image and a probe run
  is allowed.
- A relink of the contained launcher
  (`tests/network_isolation/windows_network_sandbox.cpp`) stalls the whole
  contained suite until the launcher clears; every test then reports the
  same `4551`, and none of those rows is evidence about the boundary.
- It also lands on CMake try-run probes (`cmTC_*.exe`) and build tools run
  inside nested configures; the launcher try-run and the syntax provider's
  nested build already retry for it, and a configure that fails on a
  `cmTC_*` refusal is simply re-run.
- Only a refusal that persists past the verdict is a failure, and then the
  decision is the owner's, written on [[BPR-0424]]: sign the executables,
  switch the policy off, or record the exclusion.
