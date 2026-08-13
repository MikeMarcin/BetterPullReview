---
id: BPR-0290
title: Generate and mechanically verify Dif sprite assets
status: Done
rfc: RFC-BPR-0019
depends_on: [BPR-0288, BPR-0289]
---

# Generate and mechanically verify Dif sprite assets

Generate the committed strips, descriptor, and digest inventory, then register
focused contract tests that prove reproducibility and reject every malformed or
over-budget pack relevant to RFC-BPR-0019.

## Required implementation

- Generate one-row 256 x 256 RGBA strips with exactly one transparent gutter
  cell at each end and no unreferenced content frames.
- Emit a compile-time catalog sharing the manifest schema version and digest,
  with no filesystem paths or runtime JSON parser.
- Compare two fresh regenerations with the committed generated tree byte for
  byte, including file names and absence of extras.
- Test alpha, edge clearance, baseline/body/center travel, returns, priorities,
  representative frames, descriptions, loop seam, handoff, rates, dimensions,
  frames, decoded pixels, widths, and file-byte limits.
- Register one offline asset contract in both developer and core-only CTest
  selection without adding Qt or art-generation dependencies to core-only.

## Completion evidence

- Focused positive and hostile contract tests pass and are non-vacuous.
- Clean regeneration produces zero diff from committed generated assets.
- The descriptor compiles in a narrow consumer and rejects stale manifest or
  resource digests.

## Implemented products

The committed `src/adapters/qt/assets/dif/generated/` inventory contains only:

- `dif-calm-idle.png`, a 1280 x 256 strip with three content cells;
- `dif-greet.png`, a 1792 x 256 strip with five content cells;
- `dif-rest.png`, a 768 x 256 strip with one content cell;
- `dif-sprite-catalog.hpp`, the schema-1 fixed C++ descriptor with the exact
  manifest digest and three `:/bpr/dif/` resource names; and
- `dif-sprite-digests.json`, binding the manifest, ten canonical source files,
  and all four other generated products.

The nine content cells decode to 2.25 MiB and the six fixed gutters add 1.5
MiB, for 3.75 MiB total decoded strip pixels. The generator writes only
canonical 8-bit straight-alpha RGBA PNGs, and complete-tree validation rejects
missing, extra, stale, noncanonical, or symlinked content.

## Verification evidence

- `python3 scripts/dif_assets.py validate --assets
  src/adapters/qt/assets/dif`: `DIF_ASSETS_VALID`.
- `python3 scripts/dif_assets.py verify --assets
  src/adapters/qt/assets/dif`: `DIF_ASSETS_REPRODUCIBLE`; both fresh output
  trees and the committed tree matched by relative path and byte content.
- `python3 tests/scripts/dif_asset_tests.py --cxx /usr/bin/c++`: 21 focused
  positive and hostile tests passed.
- `ctest --preset dev -R '^dif_assets_contract$' --output-on-failure`: one
  network-isolated contract passed.
- `ctest --preset core-only -R '^dif_assets_contract$' --output-on-failure`:
  the same network-isolated contract passed without linking Qt or adding an
  ordinary-build art-generation step.
- The narrow consumer syntax-checks both a fresh synthetic catalog and the
  committed production catalog as C++20. Dedicated negatives alter a valid
  manifest without regenerating and alter a digest-inventory resource hash;
  both fail as stale.
- Documentation validation returned `DOCUMENTATION_OK`, and `git diff
  --check` passed.

These results are mechanical evidence only. They do not grant final source
rights or visual acceptance; BPR-0291 retains both attended decisions.

An independent read-only `rfc-scout` reproduced validation and the 30.8-second
two-tree verification, checked the five-file digest inventory and all six
transparent gutters, compiled and ran the production catalog consumer with
strict compiler warnings, and passed the developer and core-only CTest
selections. It found no mechanical BPR-0290 blocker and explicitly left final
rights, identity, readability, and motion aesthetics to BPR-0291.
