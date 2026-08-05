---
id: BPR-0156
title: Re-establish Linux build and runtime verification
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0155]
---

# Re-establish Linux build and runtime verification

BPR-0050 recorded a full Ubuntu 26.04 x86_64 GCC 15.2 matrix: process,
protocol, Secret Service, cache, catalog, maintenance, public Qt, dependency,
and runtime evidence. That run was real. It is also no longer reproducible.

BPR-0155 found that the `#elif defined(__linux__)` branches of eight files
referred to parameters by names those parameters no longer have. The rename
that broke them landed after BPR-0050, and nothing on the macOS host compiles
those branches, so nothing failed. The identifiers are repaired and a gate now
reads them, but **reading source is not compiling it**, and compiling is not
running.

## What is missing

A Linux host or virtual machine. This one has no Linux toolchain, no container
runtime, and no emulator:

```
which docker podman colima lima  ->  none
```

The MinGW cross-compiler that gave BPR-0155 its Windows evidence has no Linux
counterpart installed, and adding one is a host change rather than a code
change.

## What must be done there

1. Configure, build, and run the `dev` and `core-only` presets on Ubuntu
   26.04 x86_64 with GCC 15.2, as BPR-0050 did.
2. Confirm the eight files BPR-0155 repaired compile, and that the repaired
   `__linux__` branches behave — `getrandom` entropy, `renameat2` publication,
   `/proc/self/fd` child paths, the Secret Service vault, and the `gh` CLI
   Linux environment.
3. Re-run the process, protocol, cache, catalog, maintenance, public Qt,
   dependency, and runtime scans.
4. Record which BPR-0050 claims are re-established and which have changed.

## What must not be done instead

The gate added in BPR-0155 must not be presented as Linux verification. It
proves that a specific class of mistake is absent from the source, and that is
all it proves. Every other way a `__linux__` branch can be wrong — a missing
header, a changed syscall signature, a behaviour difference — is invisible to
it.

Nor is the MinGW result a substitute. Windows and Linux share none of these
branches.
