---
id: BPR-0311
title: Verify device authorization on target-native Windows x64
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0036, BPR-0282, BPR-0318, BPR-0493, BPR-0495]
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

## Pre-flip target-native evidence — 2026-08-29, `bpr-win-jaegaw` at `6941a91b`

Dispatched as `task-20260829-bpr0311` to a `read-only-proof` station: no
commits, pushes, card moves, or tracker edits, and `git status --short` empty
throughout. Device capability stayed `unavailable` for the whole run;
`github_application.cpp:1158-1166` was untouched. The card is **not** terminal
and does not become so on this run.

Passed, via `ctest --test-dir build\windows-dev -C Debug --no-tests=error
--output-on-failure` — the `-C` argument being [[BPR-0492]]'s repair working:

| Row | Result |
|---|---|
| Credential Manager | 4/4 — `windows_credential_manager_contract` 1.18 s, `_native_setup`, `_native`, `_native_cleanup` |
| PE/runtime/dependency, in-tree | 2/2 — `binary_dependency_surface` 0.29 s (selects `dumpbin`), `runtime_process_surface` 1.95 s |
| RFC-0016 course selector matrix | 13/14 — the one failure is a host block, recorded below |

Release artifact `build/windows-app-release/src/app/better_pull_review.exe`,
7,517,184 bytes, sha256
`f901bf9e620f4426caf0d0aada60058464848bda879d9f7b05e52a5b7edabf0e`, built from
a **local, uncommitted** `windows-app-release` preset in the gitignored
`CMakeUserPresets.json`:

| Row | Result |
|---|---|
| Shipped-artifact secret scan | `RFC0016_TEST_INVENTORY_OK` |
| Binary dependencies | `BINARY_DEPENDENCIES_OK` |
| Private owner-only client `Iv23lil08fH3933w9MkW` | ABSENT |
| Production client `Iv23liitiYN1WSSbag50` | ABSENT — correct pre-flip, and itself evidence the capability is still unregistered here |
| `client_secret`, `QtWebEngineCore` | ABSENT |
| Vault and browser-engine symbols, via the linker map | `windows_credential_manager` present (494 hits); `macos_keychain`, `linux_secret_service`, `QWebEngine`, `QWebView`, `QtWebEngine` absent |

### Blocked, and not counted

Three rows are blocked by [[BPR-0493]]. None is a product or script defect, and
none is counted as passing:

- `check_device_selection`'s zero-request probe — `A_EXIT=1073751882`, "blocked
  by your organization's Device Guard policy".
- `native_https_device_authorization_contract` — `B_EXIT=8`,
  `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551`.
- `verify_runtime_surface.py` against the Release artifact — `C_EXIT=1`,
  `OSError: [WinError 4551] An Application Control policy has blocked this
  file`, which is the plain-language attribution for the ctest `win32=4551`
  too: same policy, same code, reached through `Popen` rather than ctest.

So `DEVICE_FLOW_STAGE:unavailable` / `DEVICE_FLOW_REQUESTS:0` has **no** direct
target-native evidence from this run. The absent production client identifier
in the artifact is corroborating, not equivalent, and the station explicitly
declined to offer it as that row. That distinction is preserved here.

### This card now depends on BPR-0493

The station's judgment, which this card adopts: a two-phase run does not satisfy
the ledger's `--fresh` intent. The course's own docstring reasons that the
claims "cannot be proved by a build tree that has been incrementally patched for
a week", and a resumed tree is a smaller instance of exactly that. For the
*unavailable* rows the risk is low, since a wrong answer fails loudly. For the
*enabled* flip course it is not acceptable: that course exists to prove a
one-time selection change against a clean artifact, and a half-fresh tree is
precisely where a stale object could still carry the pre-flip constant.

BPR-0311 therefore cannot close on this host while Smart App Control enforces,
regardless of who holds write authority. That is recorded in `depends_on`.

### Delegation audit

`audit_delegation.py` reports "no session transcripts in
`C:\code\git\bpr-windows`" with and without `--allow-missing`; transcripts live
outside the checkout on that host. Station's hand count for the run: 1
`rfc-scout`, which spawned 1 `rfc-utility` descendant; 0 `rfc-worker` (station
is read-only-proof, no writes permitted); 0 reviewer (no closeout to review).

## The `windows-app-release` preset has landed — 2026-08-29

The card requires this preset to land in the same commit as the card. It could
not: the station that built and measured it is `read-only-proof`, so the
integrator landed it on `main` instead. The product owner approved that
substitution explicitly, on the reasoning that the requirement exists to stop
the preset landing untethered from its evidence — so the commit names this card
and quotes the exact artifact the preset produced:

```text
build/windows-app-release/src/app/better_pull_review.exe
sha256 f901bf9e620f4426caf0d0aada60058464848bda879d9f7b05e52a5b7edabf0e
7,517,184 bytes — clean configure, 270/270 build, target-native
```

It is `app-release`'s cache variables plus `windows-dev`'s host condition, on
`_ninja-common`. **Ninja rather than the multi-config Visual Studio generator**
was the product owner's decision, taken with the tradeoff stated: the shipped
Windows artifact now comes from a different generator than the one developers
build with under `windows-dev`. Two things bound that cost, and both were
measured rather than assumed — `/MAP` is emitted under `if(MSVC)` regardless of
generator, so the composed-host and platform-course symbol evidence is
identical either way; and [[BPR-0492]]'s `tree_configuration()` repair means
multi-config was no longer *blocked*, only heavier, so this was a preference
rather than a forced move. The station recorded its own lean toward Ninja as a
preference and explicitly declined to offer it as a target-native finding.

[[BPR-0315]] and [[BPR-0035]]'s dormant-CLI-symbol row also consume this
preset, so the choice is not BPR-0311's alone to live with.

The ledger's Windows command names `--release-preset windows-app-release`; that
preset now exists in `CMakePresets.json` rather than only in a gitignored
`CMakeUserPresets.json`.

## Pre-flip course, second attempt — 2026-09-01, station `bpr-win` at `86659977`

Dispatched as `task-20260902-bpr0495-win` to a `lane-commit` station. **The
card does not become terminal on this run and the flip was not taken.**
`src/app/src/github_application.cpp:1158-1166` is untouched — still
`#if defined(__APPLE__)` — and `git diff HEAD -- src/` is empty.

The host condition that stopped the 2026-08-29 run is gone. Smart App Control
is now off:

```text
HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 0
Win32_DeviceGuard.UsermodeCodeIntegrityPolicyEnforcementStatus            = 0
```

against `1` and `2` on 2026-08-29. Every row the last run recorded as blocked
by [[BPR-0493]] is therefore re-measurable, and two of the three now pass.

### The exact ledger command, run twice

| Run | Environment | Duration | Outcome |
|---|---|---|---|
| 1 | ordinary shell | 13 m 32 s | `configure-failed:windows-app-release` — `CMake was unable to find a build program corresponding to "Ninja"`, `CMAKE_C_COMPILER not set` |
| 2 | under `vcvars64.bat` | 12 m 04 s | `verify_runtime_surface.py:application exited before runtime inspection` |

Run 2 reached step nine of ten. Steps one through eight passed, which is six
steps further than any Windows run has ever reached; the per-step table and the
configuration-resolution evidence are on [[BPR-0492]].

### Rows that now pass, target-native

| Row | Result |
|---|---|
| Fresh `windows-dev` configure and Debug build | passed inside the course, `--fresh` honoured for both trees in one pass — no two-phase resume |
| Test inventory under multi-config | `RFC0016_TEST_INVENTORY_OK` via `--build-config Debug` |
| RFC-0016 course selector matrix | **14/14, "100% tests passed out of 14"** — was 13/14 on 2026-08-29 |
| `native_https_device_authorization_contract` | **Passed, 0.12 s** — the row that failed with `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551` under Smart App Control |
| Qt presentation at three scales | `qt_device_authorization_scale_100`/`_125`/`_200` all passed |
| Fresh `windows-app-release` configure and Release build | passed under the developer environment |
| Shipped-artifact secret scan | `RFC0016_TEST_INVENTORY_OK` |
| Private owner-only client `Iv23lil08fH3933w9MkW` | ABSENT |
| Vault and browser-engine symbols, read by the course itself | `windows_credential_manager` present (301 demangled hits); `macos_keychain`, `linux_secret_service`, `QWebEngine`, `QWebView`, `QtWebEngine` absent — see [[BPR-0494]] |
| Binary dependencies | `verify_binary_dependencies.py` passed inside step nine, before the runtime probe failed |

Release artifact under test, built by run 2 from the committed
`windows-app-release` preset:

```text
build/rfc0016-windows-enablement/release/src/app/better_pull_review.exe
7,517,184 bytes
sha256 0112e65e7ac6bf1563506b8f99cf66748f62803102f1648ec9639be1ea207190
```

Same size as the 2026-08-29 artifact, different hash. MSVC embeds a build
timestamp and a fresh PDB signature; this is not a reproducibility claim.

### The blocking row, and why it is not the product

`verify_runtime_surface.py` refused with `application exited before runtime
inspection`. Reproduced by hand: the shipped artifact, launched directly,
exits immediately with `-1073741515` = `0xC0000135` `STATUS_DLL_NOT_FOUND` and
prints nothing.

The repository already knows this exactly. `cmake/BprTestIsolation.cmake`
prepends Qt6::Core's DLL directory to `PATH` and sets `QT_QPA_FONTDIR` for
every registered test, with the comment:

```text
A Windows loader finds a shared Qt only on PATH: nothing deploys the DLLs
beside the executables ... Without this every Qt-linked test exits before
main with STATUS_DLL_NOT_FOUND (BPR-0430).
```

That applies to ctest tests. The platform course launches the shipped artifact
outside ctest — in `check_runtime_and_dependencies` and again in
`check_device_selection` — so neither gets it. This is the same
already-solved-elsewhere shape as [[BPR-0492]] and [[BPR-0494]], and it is the
third consumer this course missed. The repair is in `scripts/**`, which this
station may not commit, so it is reported to the integrator rather than
attempted here.

### Device selection: direct evidence at last, but not the course's own row

With Qt's directory prepended by hand and `QT_QPA_PLATFORM=offscreen`, the
shipped Release artifact answers:

```text
DEVICE_FLOW_HOST_COMPOSED:true
DEVICE_FLOW_PRESENTED:true
DEVICE_FLOW_STAGE:unavailable
DEVICE_FLOW_REQUESTS:0
DEVICE_FLOW_UNAVAILABLE_OK
```

exit 0 — every assertion `check_device_selection(expected="unavailable")`
makes, and `verify_runtime_surface.py` answers `RUNTIME_SURFACE_OK` under the
same `PATH`.

The 2026-08-29 record said plainly that
`DEVICE_FLOW_STAGE:unavailable` / `DEVICE_FLOW_REQUESTS:0` had **no** direct
target-native evidence, and declined to offer the absent production client as a
substitute. That gap is now filled by direct measurement of the shipped
artifact on the target. It is still **not** the course's own row: the course
never reached step ten, and a hand launch with a repaired `PATH` is not the
`--fresh` isolated course the ledger requires. Both facts are recorded because
they are different claims — the product behaves correctly, and the course
cannot yet say so itself.

### What has to happen before the flip

1. The Qt-on-`PATH` defect in the course's direct-launch steps is repaired in
   `scripts/**` by the integrator.
2. The exact ledger command is re-run `--fresh` and reaches
   `RFC0016_PLATFORM_COURSE_OK` with `--expected-device-selection unavailable`.
3. Only then the flip, in one commit with the complete enabled-course rerun at
   `--expected-device-selection enabled --fresh`.

[[BPR-0495]] is Done as of this run, so of this card's `depends_on` only
[[BPR-0493]] remains open. Its host condition no longer obtains — Smart App
Control is off and a single-pass `--fresh` run completed twice — but its
required final state asks for the supported Windows procedure to be *written
down*, and that is still unwritten. Two things belong in it, both found today:
the ledger's Windows command needs a Visual Studio developer environment, for
Ninja and for `undname`, and neither the ledger nor the operator doc says so.
