---
schema: "implement-rfc.card.v1"
id: "BPR-1125"
title: "Windows: verify_dif_asset_boundaries.py refuses the dev and core-only graphs because dif_assets_contract runs uncontained there"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win while verifying BPR-0974, 2026-09-25","origin_card":"BPR-0974","related_cards":["BPR-0974","BPR-0412","BPR-0362"],"reservation":"docs/issues/_reservations/BPR-1125--BPR-1125--2102331b05c1.json"}
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
