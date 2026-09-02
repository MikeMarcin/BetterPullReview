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
