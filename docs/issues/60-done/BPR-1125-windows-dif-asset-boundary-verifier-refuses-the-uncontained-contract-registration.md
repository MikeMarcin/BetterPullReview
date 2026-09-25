---
schema: "implement-rfc.card.v1"
id: "BPR-1125"
title: "Windows: verify_dif_asset_boundaries.py refuses the dev and core-only graphs because dif_assets_contract runs uncontained there"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win while verifying BPR-0974, 2026-09-25","origin_card":"BPR-0974","related_cards":["BPR-0974","BPR-0412","BPR-0362"],"reservation":"docs/issues/_reservations/BPR-1125--BPR-1125--2102331b05c1.json"}
acceptance: [{"criterion":"AC1 On Windows the verifier accepts dif_assets_contract's pinned uncontained registration (compiler probe, --cxx, the bpr_windows_uncontained label, exactly once), keeps refusing a cut or duplicated registration and a contained registration without its launcher, and grows no second exemption list","outcome":"pass","revision":"350e4d6e","evidence":{"kind":"command","detail":"_validate_ctest_registration reads the graph's backend from its isolation-manifest --backend and the pin from verify_test_network_isolation.WINDOWS_UNCONTAINED_TESTS (imported). With the pin removed (temporary edit) the station's graphs are refused again ('lacks run_network_isolated_test.py'), so the verifier reads the owner's list. Cut, duplicated, compiler-less, launcher-on-Windows and contained-without-launcher registrations are each refused in the contract.","path":"docs/issues/60-done/BPR-1125-windows-dif-asset-boundary-verifier-refuses-the-uncontained-contract-registration.md#fixed-and-verified-bpr-main-win-2026-09-25-350e4d6e"}},{"criterion":"AC2 python scripts/verify_dif_asset_boundaries.py --source . --dev-build build/dev --core-build build/core-only --app-build build/windows-app-release prints DIF_ASSET_BOUNDARIES_OK on the Windows station","outcome":"pass","revision":"350e4d6e","evidence":{"kind":"command","detail":"JaegAW: DIF_ASSET_BOUNDARIES_OK over build/dev and build/core-only (79a96104 plus this change) and build/windows-app-release (BPR-0974's tests-disabled trial of the 020f9ed5 product tree). HEAD's verifier over the same trees: 'DIF_ASSET_BOUNDARIES_ERROR: dif_assets_contract registration lacks run_network_isolated_test.py'."}},{"criterion":"AC3 dif_assets_contract gains a synthetic Windows-shaped registration case, accepted, with its cut variants refused; it passes in dev and core-only, and macOS and Linux keep their contained expectation","outcome":"pass","revision":"350e4d6e","evidence":{"kind":"command","detail":"test_boundary_verifier_accepts_the_pinned_windows_registration (dev and core-only) and test_boundary_verifier_refuses_cut_or_contradicting_registrations (ten variants x two profiles, Linux-backend variants keep the contained expectation). dif_assets_contract passes via CTest in build/dev (186.4 s) and build/core-only (194.3 s). Against HEAD's verifier (temporary swap) the accepting case errors and the refusal cases fail.","path":"docs/issues/60-done/BPR-1125-windows-dif-asset-boundary-verifier-refuses-the-uncontained-contract-registration.md#fixed-and-verified-bpr-main-win-2026-09-25-350e4d6e"}}]
---
# BPR-1125: Windows: verify_dif_asset_boundaries.py refuses the dev and core-only graphs because dif_assets_contract runs uncontained there

## Finding

The Dif selected-graph boundary verifier
([`scripts/verify_dif_asset_boundaries.py`](../../../scripts/verify_dif_asset_boundaries.py))
cannot pass on Windows. `docs/development/build-and-test.md` says to run it
before closing an asset or a consuming application revision.

Its `dev` and `core-only` profiles read `tests/CTestTestfile.cmake` and require
the `dif_assets_contract` registration to carry `run_network_isolated_test.py`
and the `bpr_network_isolated` label (`_validate_ctest_registration` and
`_validate_core_ctest_surface`). On Windows that row is registered
`WINDOWS_UNCONTAINED` (`tests/CMakeLists.txt`), and
`scripts/verify_test_network_isolation.py` pins the exemption as
`child-process-denied`: the contract compiles a probe with the configured
compiler, and the AppContainer denies the child process ([[BPR-0362]],
[[BPR-0412]]). So on Windows the registration has no launcher and carries the
`bpr_windows_uncontained` label. The verifier reads that as a cut registration
and refuses both graphs.

Observed at `85e522d8` on JaegAW (MSVC 19.51, Qt 6.11.2), with `build/dev` and
`build/core-only` built from the same checkout. Both profiles refuse with:

```text
dif_assets_contract registration lacks run_network_isolated_test.py
```

The `app-release` profile has no CTest surface, and it passes against the
tests-disabled Windows graph. That includes the bundled-resource layout
[[BPR-0974]] added.

This predates BPR-0974. The row has been uncontained on Windows since the
exemption was pinned; nobody ran the verifier's tests-enabled profiles on a
Windows tree after that.

## Acceptance

- AC1: On Windows, the verifier accepts `dif_assets_contract`'s pinned
  uncontained registration: the compiler probe, the `--cxx` argument and the
  `bpr_windows_uncontained` label, exactly once. It keeps refusing a cut or
  duplicated registration, and a contained registration without its launcher.
  The exemption's set and cause stay owned by
  `scripts/verify_test_network_isolation.py`; the verifier does not grow a
  second list.
- AC2: `python scripts/verify_dif_asset_boundaries.py --source . --dev-build
  build/dev --core-build build/core-only --app-build build/windows-app-release`
  prints `DIF_ASSET_BOUNDARIES_OK` on the Windows station.
- AC3: `dif_assets_contract` gains a synthetic Windows-shaped registration
  case, accepted, with its cut variants refused. It passes in `dev` and
  `core-only`, and macOS and Linux keep their contained expectation.

## Fixed and verified, bpr-main-win, 2026-09-25 (`350e4d6e`)

Verified as `d7df7135` on `79a96104`. It was rebased onto origin's macOS test-only commits `053abefe` to `feabac34` and pushed as `350e4d6e`: the same patch (patch-id `aee53c93`).

**The verifier now expects the registration the graph's own backend and the
pin call for** (`scripts/verify_dif_asset_boundaries.py`,
`_validate_ctest_registration`). It follows `bpr_add_network_isolated_test`
(`cmake/BprTestIsolation.cmake`): only the `windows-appcontainer` backend
drops the launcher, and only for a row
`scripts/verify_test_network_isolation.py` pins. Both facts are read, and
neither is restated:

- **The backend** comes from the graph itself: the `--backend` argument of
  its `test_network_isolation_manifest` registration. The `dev` and
  `core-only` trees on this station both state `windows-appcontainer`. A
  graph that states no backend, or two, is refused.
- **The pin** is `WINDOWS_UNCONTAINED_TESTS`, loaded from
  `verify_test_network_isolation.py`, the one list that owns the set and its
  causes. The Dif verifier grows no second list.

The registration is read as its own `add_test("dif_assets_contract" ...)`
line, exactly once, with its properties line. The old check looked for each
marker anywhere in the file. The command must name `dif_asset_tests.py`,
`--cxx` and a compiler after it. Then:

- **pinned, on `windows-appcontainer`:** no launcher, and the
  `bpr_windows_uncontained` label without `bpr_network_isolated`;
- **everything else,** macOS and Linux among them: the launcher, and the
  `bpr_network_isolated` label without the uncontained one.

**AC1 and AC2, on the station** (JaegAW, MSVC 19.51, Qt 6.11.2):

| Run | Result |
| --- | --- |
| The command in AC2, over `build/dev` and `build/core-only` at `79a96104` plus this change, and `build/windows-app-release` (the tests-disabled trial of [[BPR-0974]]'s product tree, `020f9ed5`) | `DIF_ASSET_BOUNDARIES_OK` |
| The same command with `HEAD`'s verifier | `DIF_ASSET_BOUNDARIES_ERROR: dif_assets_contract registration lacks run_network_isolated_test.py`, the finding |
| The same command with `dif_assets_contract` removed from `WINDOWS_UNCONTAINED_TESTS` (a temporary edit, restored) | the same refusal. The verifier reads the pin; it does not assume Windows is uncontained |

**AC3, the contract** (`tests/scripts/dif_asset_tests.py`). The synthetic
graph builder now states a backend (Linux by default, so the existing cases
keep the contained expectation). Two new cases:

- `test_boundary_verifier_accepts_the_pinned_windows_registration`: a
  Windows-shaped `dev` and `core-only` graph is accepted. It first asserts
  that the pin still names `dif_assets_contract`.
- `test_boundary_verifier_refuses_cut_or_contradicting_registrations`: ten
  variants in both profiles, each refused with its reason:
  - on Windows: the label cut; labelled contained; the compiler probe cut;
    the compiler cut; duplicated; registered through the launcher;
  - on Linux: registered uncontained; the contained label without its
    launcher; the launcher labelled uncontained;
  - a graph that states no backend.

| Run | Result |
| --- | --- |
| `dif_assets_contract` in `build/dev` (CTest, Windows-uncontained as registered) | pass, 186.4 s |
| `dif_assets_contract` in `build/core-only`, rebuilt at `79a96104` | pass, 194.3 s |
| The five registration cases with this verifier | 5 of 5 |
| The same cases with `HEAD`'s verifier (a temporary swap, restored) | the accepting case errors and the refusal cases fail |

The logs are station records under `build/logs/bpr-1125-main-win/`, not
committed. Every criterion is met.
