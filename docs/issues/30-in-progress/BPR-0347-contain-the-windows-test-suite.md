---
id: BPR-0347
title: Contain the Windows test suite in an AppContainer
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0339]
---

# Contain the Windows test suite in an AppContainer

[[BPR-0235]]'s first remaining criterion is to replace Windows' pre-launch
refusal with a real whole-suite boundary. Until now `WIN32` selected
`FailClosedAutomatedTestIsolation.cmake`, so every configured test refused
before it started: [[BPR-0336]] recorded 84 of 84 failing identically with
`Automated test launch refused: the Windows default-deny network boundary is
unavailable`, and zero test binaries executed. No Windows behavioural claim
can be made from this host while that holds.

Windows offers an unprivileged process no equivalent of a network namespace or
of `sandbox-exec`. The boundary that does exist is the AppContainer: a process
entered with **zero** capabilities gets neither `internetClient` nor
`privateNetworkClientServer`, and the Windows Filtering Platform then refuses
every external connect with `WSAEACCES`. The container token is inherited by
every descendant unconditionally, which is what makes a whole process tree
safe rather than just the one process CTest launched.

## Shape

`tests/network_isolation/windows_network_sandbox.cpp` is the launcher, and it
replaces the refusal script as `BPR_AUTOMATED_TEST_ISOLATION_PREFIX`. The
backend identifier moves from `windows-unavailable` to `windows-appcontainer`.

Three properties of the design are worth naming, because each answers a way
the boundary could have failed open or failed incoherently:

- The launcher refuses unless `MPSSVC` is running. AppContainer network
  restriction is enforced by the Windows Filtering Platform, which that
  service hosts; with it stopped the boundary can fail open. An unproven
  boundary refuses the test rather than running it unprotected.
- The caller's command line is forwarded verbatim with only this launcher's
  own token removed. Rebuilding one from `argv` would have to re-quote and
  could change what the test actually receives.
- `CreateProcessW` reports success even when the container cannot read the
  image it was handed, and the launch then fails incoherently much later. So
  the filesystem grants are established at configure time, before any test is
  registered, rather than discovered per test.

`cmake/WindowsTestIsolationSetup.cmake` settles the two machine facts a
contained child cannot establish for itself. It derives the container SID by
running the launcher's own single-argument setup query — the SID is a hash of
the container name that only the Windows API computes, and configure time
needs it before any target has been built — then grants that SID read access
to the sources and the resolved Qt prefix and modify access to the build tree.
`icacls /grant:r` replaces this SID's own entry and leaves every other
principal alone, so repeated configures are exactly idempotent.

## The one manual step

Windows blocks loopback for an AppContainer until the machine carries a
loopback exemption for it, and the suite reaches its own fixtures over
loopback. Creating that exemption requires elevation, which a build must never
take on the owner's behalf. Configure therefore checks it first — before
anything is granted, so a refusal leaves no side effect on the owner's source
tree — and refuses with the exact elevated command:

```
CheckNetIsolation.exe LoopbackExempt -a -n=BprAutomatedTest
```

## Verified so far

- The launcher compiles under MSVC 14.51 at C++23 with the project's warning
  and hardening settings, and `clang-format --output-replacements-xml` reports
  zero replacements for it.
- `cmake --preset core-only` runs the setup query, creates the AppContainer
  profile on a machine that never had it, and derives
  `S-1-15-2-2501375856-1686288671-3684386043-353760509-1080245170-1858810467-2136715157`.
- With no exemption present, configure refuses at the gate above and names
  that command. This is the designed refusal, observed rather than assumed.
- `python3 scripts/verify_documentation.py --root .` reports `DOCUMENTATION_OK`.
- Only the `elseif(WIN32)` branch changed, so macOS and Linux are untouched.

## Still required

This card stays in `30-in-progress/`; the slice is not coherent yet.

- `scripts/verify_test_network_isolation.py` still knows only
  `windows-unavailable`, so `test_network_isolation_manifest` raises
  `unknown backend: windows-appcontainer`. It needs the launcher prefix case.
- `tests/scripts/network_isolation_counterexamples.py` carries a denial shape
  for macOS and Linux only, so the `external` and `child` probes return
  `NETWORK_ISOLATION_EXTERNAL_UNKNOWN_BACKEND`. Windows denies with
  `WSAEACCES`, which is its own shape and must be declared as one rather than
  folded into the existing set.
- `loopback_attempt` binds an `AF_UNIX` socket, which CPython does not expose
  on Windows. The loopback probe needs a Windows answer.
- `cmake/FailClosedAutomatedTestIsolation.cmake` is now referenced only by the
  verifier's `windows-unavailable` branch and should go with it.
- No test has executed on Windows. The descendant and application matrix
  [[BPR-0235]] requires is unrun, and the loopback exemption is not yet on
  this machine.
