---
id: BPR-0492
title: The platform course cannot run under a multi-config generator
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The platform course cannot run under a multi-config generator

## Why

`docs/verification/RFC-BPR-0016.md` gives [[BPR-0311]] an exact Windows command:

```text
py -3 scripts/verify_rfc0016_platform_course.py --platform windows \
    --dev-preset windows-dev --release-preset windows-app-release \
    --build-root build/rfc0016-windows-enablement \
    --expected-device-selection unavailable --fresh
```

`windows-dev` is the only preset in `CMakePresets.json` that names a generator,
`Visual Studio 18 2026`, and the only one that sets no `CMAKE_BUILD_TYPE` —
because a multi-config generator chooses the configuration at build time
instead. `scripts/verify_rfc0016_platform_course.py` assumes the single-config
shape everywhere it touches a build tree:

- `configure_and_build` runs `cmake --build <dir>` with no `--config`, so a
  multi-config tree builds its default configuration whatever the preset meant.
- `check_course_selectors` runs `ctest --test-dir <dir>` with no `-C`, so ctest
  has no configuration to select tests from.
- `application_binary` probes `src/app/BetterPullReview.exe` and never
  `src/app/<Config>/BetterPullReview.exe`, which is where a multi-config
  generator actually writes it.

The course therefore cannot reach its first assertion on Windows. The failure
is `application-binary-missing:release` at best, and a silently wrong
configuration at worst — a Debug tree scanned as though it were the shipped
Release artifact, which is the more dangerous outcome because it does not look
like a failure.

This is the same class of defect as done [[BPR-0462]], where the inventory
scripts read an empty manifest under multi-config. That one was found and fixed
on the manifest side; this one sits in the course driver and was never
exercised, because no Windows station had ever run the course.

Missing `windows-app-release` is **not** part of this card. Adding and
recording that preset is [[BPR-0311]]'s own work and has to land with that
card's commit.

## Required final state

- The course resolves the build configuration for each tree it configures and
  passes it to `cmake --build`, to `ctest`, and to the artifact probe, so that
  the tree built, the tests run, and the binary scanned are the same
  configuration.
- Multi-config is detected from the configured tree rather than assumed from
  the platform or the preset name; a Ninja tree on Windows and a Visual Studio
  tree both behave correctly.
- A configuration mismatch fails loudly. Scanning a Debug artifact while
  claiming a Release course is the outcome this card exists to make impossible.
- macOS and Linux single-config behavior is unchanged, proven by the existing
  course still passing on this host.
- The Windows course reaches its first real assertion rather than
  `application-binary-missing`, proven target-native.

## Notes

Found 2026-08-29 while dispatching [[BPR-0311]] to `bpr-win-jaegaw`. The
station read the script and reported the three gaps; they were then confirmed
independently against `main` before this card was written. `scripts/*` is on
the station's must-not-commit list, so the repair is the integrator's on
`main` and the station's reproduction is the evidence it is verified against.

## Repair and what proves it — 2026-08-29

`tree_configuration()` resolves each configured tree from its own
`CMakeCache.txt` rather than from the platform or the preset name:
`CMAKE_CONFIGURATION_TYPES` present means multi-config and the intended
configuration is named downstream; absent means single-config and
`CMAKE_BUILD_TYPE` must already equal it. The single-config branch checks
instead of assuming, which is what closes the quiet failure — a Release course
run against a Debug tree now stops at `configuration-mismatch` rather than
scanning the wrong bytes and passing.

The resolved configuration reaches all four places that needed it:
`cmake --build --config`, `ctest -C`, the artifact probe's
`src/app/<Config>/` root, and — the one the original card missed —
`verify_rfc0016_test_inventory.py --build-config`. The inventory script has
carried that argument since [[BPR-0462]], with its own help text saying it is
"required for a multi-config generator, whose inventory is empty without one."
The course never passed it, so on Windows the inventory step refused. Passing
it is what lets that gate do its job rather than stop at a broken invocation.

The multi-config artifact probe puts the configuration directory ahead of the
flat path, because a stale flat leftover can coexist with a fresh
per-configuration build and picking it up would scan the wrong bytes.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Single-config trees resolve to no named configuration | `tree_configuration` over real `build/dev` and `build/app-release` | `None` for both; `CONFIG_TYPES` empty, `BUILD_TYPE` Debug/Release as expected | this commit |
| A mismatched single-config tree fails loudly | `tree_configuration(build/dev, "Release")` on the real tree | `configuration-mismatch:tree=Debug:course=Release` | this commit |
| The real macOS artifact is still found | `application_binary(build/app-release, release=True, configuration=None)` | `build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | this commit |
| Multi-config trees name the configuration | Synthetic caches with `CMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo` | Returns `Release` and `Debug`; a tree lacking the configuration fails `configuration-unavailable`; a missing cache fails `cmake-cache-missing` | this commit |
| The probe prefers the configuration directory | Both `src/app/BetterPullReview.exe` and `src/app/Release/BetterPullReview.exe` present | Picks the `Release/` one; with `configuration=None` picks the flat one | this commit |
| The Windows course reaches a real assertion | Ledger's exact `py -3` command on `bpr-win-jaegaw` | **Pending** — target-native, and the reason this card is not terminal | Pending |

The macOS half is proven on this host. The Windows half is not provable here at
all, so the card moves to `21-ready-win/` rather than to `60-done/`: the repair
is committed and unproven on the only platform that motivated it.

## Correction: the missing `--build-config` was never a vacuous pass — 2026-08-29

This card and its commit `db011a54` claimed that omitting `--build-config`
would let the inventory gate pass vacuously on Windows, and called that a worse
outcome than `application-binary-missing`. That was wrong, and `bpr-win-jaegaw`
disproved it target-native at HEAD `6941a91b`:

```text
py -3 scripts\verify_rfc0016_test_inventory.py --build-dir build\windows-dev --repository-root .
  -> rfc0016-test-inventory:fail:ctest-inventory-empty   exit 1
py -3 ... --build-config Debug
  -> RFC0016_TEST_INVENTORY_OK                           exit 0
```

Confirmed independently here by reading the script: `registered_tests` raises
`InventoryFailure("ctest-inventory-empty")` on an empty result, with the comment
"an empty inventory is a broken invocation, never a graph fact."
[[BPR-0462]] had already hardened exactly this. The gate fails closed, so the
course would have stopped at `check_inventory`'s `inventory-failed:` — loudly,
and before `application-binary-missing` — rather than proving nothing quietly.

The repair still stands and is still worth having: passing `--build-config` is
what makes the inventory *run* instead of refuse. But it fixed a fail-closed
refusal, not a silent hole, and the claim is corrected here rather than left
standing as a defect that never existed. The one genuine silent-failure risk
this card closes is the other one — a Release course scanning a Debug tree,
which `tree_configuration`'s single-config branch now refuses.

## Target-native results — 2026-08-29, `bpr-win-jaegaw` at `6941a91b`

The repaired code paths were never reached. The exact ledger command failed at
the first step, 6.5 minutes in, at `configure-failed:windows-dev`. The cause is
a host condition rather than anything in this repository: Smart App Control is
on and enforcing (`VerifiedAndReputablePolicyState = 1`,
`UsermodeCodeIntegrityPolicyEnforcementStatus = 2`), and it blocked the freshly
linked `katehighlightingindexer.exe` that the syntax-highlighting dependency
generates during configure. That is the [[BPR-0424]] quirk. It is tracked
separately as [[BPR-0493]], because its consequence is structural rather than
incidental.

Diagnosing it required a hand reproduction, because `configure-failed:{preset}`
discarded the captured output that named Device Guard outright — while the
ctest and inventory paths already carried a tail. Both `configure-failed` and
`build-failed` now carry one too.

Pre-flip Windows rows that do pass at this HEAD, via
`ctest --test-dir build\windows-dev -C Debug` — which is the repaired `-C`
argument working as intended:

- Credential Manager 4/4; PE/runtime/dependency 2/2 (`binary_dependency_surface`
  selecting `dumpbin`, `runtime_process_surface`).
- The RFC-0016 course selector matrix 13/14, the single failure being
  `native_https_device_authorization_contract` with
  `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551` — the same Device Guard
  condition, not the product.

Device capability stayed `unavailable` throughout and
`github_application.cpp` was untouched, as the dispatch required.
