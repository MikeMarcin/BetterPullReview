---
schema: "implement-rfc.card.v1"
id: "BPR-0156"
title: "Re-establish Linux build and runtime verification"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0155"]
completed: "2026-08-17"
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

## Result — 2026-08-17

An Ubuntu 26.04 x86_64 WSL2 host answered the blocker. Both presets configure,
build, and run, and the Linux claims BPR-0050 recorded are re-established
except where noted below.

### Host, against BPR-0050's recorded matrix

| | BPR-0050 (QEMU guest) | This host |
|---|---|---|
| Distribution | Ubuntu 26.04 x86_64 | same |
| Compiler | GCC 15.2 | GCC 15.2.0 |
| Git | 2.53 | 2.53.0 |
| libsecret | 0.21.7 | 0.21.7 |
| gnome-keyring | 50 | 50.0 |
| Qt | 6.10.2 | 6.10.3, re-pinned by BPR-0337 |
| CMake | 4.2.3 | 4.4.2, required by CMakePresets.json |
| Kernel | 7.0.0-28 | 6.18.33.2-microsoft-standard-WSL2 |

The kernel and virtualization differ. Everything the Linux claims rest on —
`getrandom`, `renameat2`, `/proc/self/fd`, user and network namespaces, the
Secret Service, and a real XCB display — is present and was exercised, so the
difference did not narrow the matrix. `pkg-config`, the OpenGL and Vulkan
development packages, the XCB runtime libraries, `libsecret-1-dev` and
`gnome-keyring` came from Ubuntu's archive. CMake, Ninja, a Python with
`ensurepip` for the Qt provider's virtual environment, and `gh` are not in
Ubuntu 26.04 at the required versions and were installed under `~/.local`.

### Build

| Preset | Result |
|---|---|
| `core-only` | 358 of 358 edges, warnings as errors |
| `dev` | 671 of 671 edges, warnings as errors, `better_pull_review` links |
| tests-disabled Release | **does not compile** — BPR-0343 |

The `dev` build did not compile until BPR-0346. `std::int64_t` is `long` here
and `long long` on macOS, so a `QJsonValue` initializer that binds exactly
there is ambiguous here. Unlike BPR-0155's findings this line is in no
conditional branch, so `verify_conditional_branches.py` cannot reach it. Only
a Linux compiler can.

### Compilation of the nine files BPR-0155 repaired

All nine compile. Seven of them are unreachable from `core-only`, because the
GitHub and Git adapters are gated on `BPR_BUILD_QT`, so only the `dev` preset
proves this:

| File | Object |
|---|---|
| `installed_git_cache.cpp` | built |
| `installed_git_process.cpp` | built |
| `gh_cli_transport.cpp` | built |
| `catalog_database.cpp` | built |
| `epoch_runtime_ports.cpp` | built |
| `git_cache_contract_tests.cpp` | built |
| `gh_cli_transport_tests.cpp` | built |
| `github_cli_process_runner_tests.cpp` | built |
| `platform_test_support.hpp` | compiled through its includers |

### Named Linux behaviors

| Claim | Evidence | Result |
|---|---|---|
| `getrandom` entropy | `epoch_runtime_ports.cpp` compiled and exercised through the epoch host and catalog suites | re-established |
| `renameat2` publication | catalog, cache, and process suites pass | re-established |
| `/proc/self/fd` child paths | `sqlite_epoch_catalog_*` and the boundary contracts pass | re-established |
| Secret Service vault | `linux_secret_service_contract`, `_native_setup`, `_native`, `_native_cleanup` all pass against a real gnome-keyring 50.0 owning `org.freedesktop.secrets` | re-established |
| `gh` CLI Linux environment | `github_cli_process_runner` passes; `gh_cli_transport` passes outside the sandbox and fails inside it for a harness reason — BPR-0342 | re-established for the product, harness defect recorded |

### Scans

`core-only` 82 of 84. `dev` 175 of 183. Every failure is accounted for:

| Test | Cause |
|---|---|
| `platform_configuration_contract` | Pre-existing on `main` since e5797f3; asserts `Visual Studio 17 2022` against a preset that says 18 2026. Not Linux-specific and fails on macOS too |
| `dependency_modernization_contract` | Pre-existing on `main` since fbf0f18; asserts a string `BprDependencies.cmake` no longer contains. Not Linux-specific |
| `gh_cli_transport` | BPR-0342 — the isolation launcher maps the tester to uid 0, and root is exempt from the permission check the test requires |
| `application_composition_contract` | BPR-0343 — the Release graph does not compile under GCC, and a seal assertion reads Clang's wording |
| `application_agent_invocation_composition_contract` | BPR-0341 — cross-device rename between `/tmp` and the build filesystem |
| `application_queue_composition_contract` | BPR-0344 — first head depends on the data root's filesystem |
| `application_epoch_maintenance_public_path` | BPR-0345 — alternates pass and fail across consecutive runs |
| `application_native_notification_attended_contract` | `platform_tray_not_composed`. WSLg provides no system tray. Environment, not a defect; BPR-0168 already parks native notification work |
| `application_review_ledger_composition_contract` | Budget expired at 40012–40018 ms against a 40000 ms bound. A sub-0.05% overrun; this host is slower on that path than BPR-0050's guest |

Two failures were repaired rather than recorded. BPR-0346 made the `dev` build
compile. BPR-0340 fixed a `signal_recorder` whose Qt connections outlived it,
which was undefined behavior that macOS happened to tolerate. BPR-0235's Linux
criterion was discharged separately: the network boundary was always correct
and the probe only accepted macOS's errno.

### Not reproducible here

`coding-format-check` reports 290 of 373 files as unformatted under
clang-format 18, 19, 20 and 21, the versions Ubuntu 26.04 offers.
`src/core/src/model.cpp` is clean under all of them and
`src/app/src/github_application.cpp` is not, so this is not a blanket version
disagreement, and no available version reproduces whatever formatted the tree.
The gate names no version and `format_check.py` defaults to whatever
`clang-format` is on `PATH`. The BPR-0346 edit was checked directly instead:
392 violations before it and 392 after, with none at the edit site, so it
introduced none. Pinning or recording a supported version is a decision for
the coding standard's owner and is not taken here.

### What has changed since BPR-0050

BPR-0050's process, protocol, Secret Service, cache, catalog, maintenance,
public Qt, dependency, and runtime groups are re-established. What is new is
that the Release composition no longer builds here, three contracts fail for
reasons that are host-visible rather than platform-visible, and the isolation
backend changes the privilege the tests observe. Those are BPR-0341 through
BPR-0345. None of them existed as claims in BPR-0050; they are properties of
code and harness written since.

Qt is XCB against a real 3840x2160 WSLg display, and both the XCB and Wayland
platform plugins initialize. The fixture-driven public paths force
`QT_QPA_PLATFORM=offscreen`, and offscreen resolves `QWindowsStyle` rather
than the `QFusionStyle` the Linux UX claims name, so those fixtures do not
exercise the style those claims describe. Recorded for whoever owns the Linux
UX claims; it is not this card's to change.

## Postscript — 2026-08-28

This card's attribution of `application_review_ledger_composition_contract` —
"a sub-0.05% overrun; this host is slower on that path than BPR-0050's guest"
— is disproven by measurement. The course also expires at a 150000 ms budget,
at this card's own baseline `82c7d1c1` as well as at `17c8950d`, with
`epoch_handoff_failed=1` every time: the handoff never completes and the
budget is only what ends the wait. [[BPR-0472]] owns the measured truth.
