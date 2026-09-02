---
id: BPR-0495
title: The syntax provider builds every Visual Studio configuration and MinSizeRel does not compile
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The syntax provider builds every Visual Studio configuration and MinSizeRel does not compile

## Why

With Smart App Control off on the JaegAW host ([[BPR-0493]]), a fresh
`windows-dev` configure gets past the `katehighlightingindexer.exe` refusal
for the first time and fails further on, for an unrelated reason.

`windows-dev` is the only multi-config preset. Under a multi-config generator
`cmake/BprSyntaxHighlightingProvider.cmake` builds and installs the vendored
KSyntaxHighlighting dependency once per entry of `CMAKE_CONFIGURATION_TYPES`
(the `foreach(configuration IN LISTS configurations)` loop near line 516),
and Visual Studio defaults that list to all four of
`Debug;Release;MinSizeRel;RelWithDebInfo`. The `MinSizeRel` build fails
deterministically — four reproductions on 2026-09-01, including a serial
non-parallel rebuild — with

```text
error C1083: Cannot open include file:
'kquicksyntaxhighlightingplugin_org_kde_syntaxhighlightingPlugin.moc'
```

The project never builds `MinSizeRel` or `RelWithDebInfo`: every preset and
every course selects Debug or Release. The provider is doing work nothing
consumes, and the work it does not need is the work that fails.

This is the third syntax-provider multi-config defect on this host.
[[BPR-0428]] taught the nested configure to see all four configurations;
[[BPR-0429]] stopped four installs landing in one library. Both made the loop
correct for the configurations it iterates. This card is about which
configurations it iterates at all.

## Required final state

- A fresh `windows-dev` configure completes on the JaegAW host with Smart App
  Control off, reaching the `bpr_syntax_execute` build/install steps for only
  the configurations the project builds.
- The provider builds and installs the dependency for exactly the
  configurations the tree can select — Debug and Release under the
  multi-config preset — and the mechanism is declared where a reader looks
  for it (the preset's `CMAKE_CONFIGURATION_TYPES`, the provider's loop, or
  both), not inferred from a default the generator happens to supply.
- Selecting a configuration the provider did not build fails loudly at
  configure or build time rather than linking against the wrong install.
- macOS and Linux single-config behavior is unchanged; the existing
  `SINGLE_CONFIGURATION` branch still runs once.
- [[BPR-0311]]'s exact ledger command
  (`verify_rfc0016_platform_course.py --platform windows ... --fresh`)
  reaches [[BPR-0492]]'s first real assertion on the Windows host.

## Non-goals

- Making `MinSizeRel` or `RelWithDebInfo` build. Nothing selects them.
- Changing the single-config `windows-app-release` path, which forwards
  `CMAKE_BUILD_TYPE=Release` and is unaffected.

## Verification

```text
cmake --preset windows-dev --fresh
cmake --build --preset windows-dev --config Debug
ctest --test-dir build\windows-dev -C Debug -R '^(syntax_classification_contract|dependency_modernization_contract)$' --output-on-failure
py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection unavailable --fresh
```

The last command is BPR-0311's; this card is done when it gets past
`configure-failed:windows-dev` and the dependency build logs under
`_bpr/ksyntaxhighlighting/.../logs/` show only the configurations the tree
selects.

## Notes

Reported 2026-09-01 by the hand-started JaegAW session working BPR-0311 in a
`main` checkout; minted here on `main` by the integrator because that session
holds no reservation. Found the same hour Smart App Control was turned off,
which is why no earlier Windows run could have reached it.

## Repair and what proves it — 2026-09-01

The mechanism is declared in the preset rather than left to the generator's
default: `windows-dev` now sets `CMAKE_CONFIGURATION_TYPES` to `Debug;Release`,
which is exactly the set every build preset, test preset, and course selects,
and the provider's loop comments say why it iterates that list. Asking Visual
Studio for a configuration outside the list fails at build time, which is the
loud failure the required final state asks for; no provider-side check was
added because the generator already refuses.

`platform_configuration_contract` pins the list exactly, so widening it is a
deliberate change to what the provider builds rather than something a default
can do silently.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A fresh multi-config configure completes with the list narrowed | `cmake --preset windows-dev -B build/rfc0016-configtypes-probe --fresh -D "CMAKE_CONFIGURATION_TYPES=Debug;Release"` on the JaegAW host, run read-only by the hand-started session in its `main` checkout before the preset change existed | exit 0, "Configuring done (226.6s)", generating done; the `C1083` failure is confined to configurations nothing consumes | probe against `32aee527`; this commit lands the same list in the preset |
| The preset declares the list and the contract pins it | `python3 tests/scripts/platform_configuration_tests.py` | 5/5, including the new exact `CMAKE_CONFIGURATION_TYPES` assertion | this commit |
| The preset file stays valid presets JSON | `python3 -c 'import json; json.load(open("CMakePresets.json"))'` | loads | this commit |
| Single-config trees are untouched | The provider's `SINGLE_CONFIGURATION` branch is unchanged; only a comment was added inside the multi-config branch | no macOS/Linux behavior change by construction; the RFC-0006/0011 focused selectors run on this host at this tree stayed green (18/18 and pending) | this commit |
| The fresh `windows-dev` configure completes from the committed preset, and only Debug/Release dependency logs exist | `cmake --preset windows-dev --fresh` on the JaegAW host from a checkout of this commit | Recorded target-native below | `6fff0a53` |

The card moves to `21-ready-win/`: the repair is committed and the remaining
row can only be produced on the Windows host. The exact ledger command for
[[BPR-0311]] then follows on the station.

## Target-native result — 2026-09-01, station `bpr-win` at `6fff0a53`

The pending row is now measured. Smart App Control is **off** on this host
(`VerifiedAndReputablePolicyState = 0`,
`Win32_DeviceGuard.UsermodeCodeIntegrityPolicyEnforcementStatus = 0`), which is
the condition [[BPR-0493]] describes and the reason the configure could get
past `katehighlightingindexer.exe` at all.

`build/windows-dev` was deleted outright before the run rather than relying on
`--fresh` alone. `--fresh` clears the cache but not `_bpr/`, and that directory
still held the four-configuration logs from the pre-repair run — so with the
tree left in place, "which logs exist" could not distinguish a narrowed build
from stale leftovers. The deletion is what makes the log listing below evidence.

| Claim | Command | Result |
|---|---|---|
| The fresh multi-config configure completes | `cmake --preset windows-dev --fresh` | exit 0, `-- Configuring done (214.8s)`, `-- Generating done (1.2s)`, `-- Build files have been written to: C:/code/git/bpr-windows/build/windows-dev`; 3 m 36 s wall. No `C1083`, and no `MinSizeRel` compile attempted |
| The committed preset supplies the list, not the generator default | `CMakeCache.txt` after the run | `CMAKE_CONFIGURATION_TYPES:UNINITIALIZED=Debug;Release` |
| The provider built and installed exactly the configurations the tree selects | `ls build/windows-dev/_bpr/ksyntaxhighlighting/6.28.1/logs/` | `syntax-highlighting-build-Debug.log`, `syntax-highlighting-build-Release.log`, `syntax-highlighting-install-Debug.log`, `syntax-highlighting-install-Release.log`, plus the single `syntax-highlighting-configure.log` and the two `extra-cmake-modules-*` logs. **No `MinSizeRel` or `RelWithDebInfo` log of either kind** |
| Selecting a configuration the provider did not build fails loudly | `cmake --build build/windows-dev --config MinSizeRel` | exit 1, `error MSB8013: This project doesn't contain the Configuration and Platform combination of MinSizeRel|x64` — the generator refuses at build time rather than linking against the wrong install |

The last row is worth stating plainly because the repair commit asserted it by
construction — "no provider-side check was added because the generator already
refuses" — and did not measure it. It is measured now, and the generator does
refuse.

### A host-state defect found on the way, and not a repository one

The first attempt at the exact command failed in 70 ms:

```text
CMake Error: Could not read presets from C:/code/git/bpr-windows:
Duplicate preset: "windows-app-release"
```

[[BPR-0311]] landed `windows-app-release` in `CMakePresets.json`, and this
checkout's gitignored `CMakeUserPresets.json` still carried the hand-built
preset of the same name from that card's earlier read-only run. Two presets
with one name make *every* preset command on the host unreadable, including
`windows-dev`. The superseded configure/build pair was removed from the
untracked file — the unrelated `windows-app-release-vs` experiment was kept —
and the command then ran as recorded above.

No card is minted: the file is machine-local, untracked, and not a repository
artifact, so there is no repository state to repair. It is recorded here because
any station whose checkout predates that preset landing will hit it, and the
error names neither the file nor the fact that one of the two presets is stale.

The card is terminal. [[BPR-0311]]'s exact ledger command follows on this
station and its result is recorded there, not here.
