---
schema: "implement-rfc.card.v1"
id: "BPR-0292"
title: "Verify Dif asset isolation and security boundaries"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: ["BPR-0290"]
---

# Verify Dif asset isolation and security boundaries

Prove that RFC-BPR-0019 remains an offline Qt-adapter asset contract and does
not smuggle runtime composition, active content, private material, or an art
toolchain into ordinary product graphs.

## Required verification

- Configure, build, and test the developer and core-only graphs from clean
  state; configure and build the tests-disabled application graph.
- Inspect selected source/resource/dependency graphs and binaries for absence
  of core Dif assets, generator execution, runtime JSON parsing, model/network
  dependencies, and RFC-BPR-0020 widget/timer/preferences.
- Audit the Dif tree for strict path inventory, PNG metadata/chunks, active or
  remote content, private paths, prompts, credentials, repository data, GitHub
  marks, and Nib/Mel/AllTheThings product material.
- Confirm provenance records the notice/SBOM disposition without adding package
  install, signing, notarization, release, or certification work.

## Completion evidence

- Direct clean commands and scoped scans pass at the proving commit.
- Cutting the CTest registration, admitting an unlisted file, selecting an art
  dependency, or exposing a private path makes the corresponding gate fail.
- No screenshot or source-art inspection is claimed as runtime composition.

## Completion evidence

The selected-graph verifier landed in `scripts/verify_dif_asset_boundaries.py`
and is exercised by the already registered, network-isolated
`dif_assets_contract`. Its 33 cases pass directly and through both developer
and core-only CTest selections. The nonvacuous corpus rejects a cut CTest
registration, a Qt application probe in core-only, a selected ImageMagick
dependency, Qt/app sources or Qt dependencies in core-only, test sources in
the tests-disabled app, Dif markers in the application or core libraries, a
network-capable asset tool, a private path in asset documentation, and a
missing prompt-authority disposition. The underlying asset contract also
rejects unlisted files, symlinks before digest reads, hostile PNG metadata,
stale output, and noncanonical inventory.

The first clean core-only replay found that
`test_network_isolation_application` was registered without its absent Qt
target. Commit `d402338ba45be036b69bfe9e5c30507b733a60dd` makes that registration
conditional on the target and makes the verifier reject its return to a
core-only graph. At that exact proving commit, a clean `git archive` source
snapshot produced these results:

- developer configure and 339-step build passed; all 182 tests passed;
- core-only configure and 178-step build passed; all 83 selected tests passed;
- tests-disabled `app-release` configure and 126-step application build passed;
- `python3 scripts/verify_dif_asset_boundaries.py --source . --dev-build
  build/dev --core-build build/core-only --app-build build/app-release`
  returned `DIF_ASSET_BOUNDARIES_OK`.

The verifier counted 218, 120, and 113 first-party translation units in dev,
core-only, and app-release respectively. It scanned the tests-disabled
application (`sha256:c4497c654fc0a0dbb253dae53c98e26fbf98482f15a60f1324c65776e1b32ad9`)
and three core-only first-party libraries (combined digest
`sha256:6deda1ba8831796bfe73c5ff405fff57dbca1a9e10e350cf26b2dc62a3054470`)
without finding a Dif resource, catalog, manifest, strip, or manifest-digest
marker. It also scanned 268 runtime/package source files without finding an
RFC-BPR-0019 asset selection or RFC-BPR-0020 composition.

All 18 admitted asset files pass the strict inventory and metadata validator.
The 13 PNGs contain no admitted ancillary metadata or byte-stream private,
credential, remote, or active-content marker. The exact ImageGen prompt is
deliberately retained only in the asset README as creation provenance; the
README states that checked-in PNG bytes, not prompt replay, are authoritative.
The offline tool imports only its declared Python-standard-library surface and
contains no model, HTTP, or subprocess capability.

Provenance records `notice_required: false` and a nonempty repository-only
notice disposition. No package manifest, install, signing, notarization,
release, or SBOM-generation work was added. Visual originality, GitHub-mark
absence, and foreign-character exclusion remain the exact attended BPR-0291
decisions; this automated gate does not claim them or runtime composition.

## Independent review disposition

An `rfc-scout` independently inspected the three selected graphs, source and
resource ownership, binaries, asset contract, provenance, and packaging
surface. Its material findings were the missing three-graph gate, absent core
binary scan, ambiguous prompt treatment, package no-impact evidence, and the
dirty-checkout developer failure. The committed verifier now covers the graph,
core artifact, all-file hygiene, prompt-disposition, and notice boundaries.
The exact clean archive replay removes the unrelated dirty-checkout failure.
The scout's stated attended and package-certification limitations remain
explicit above.
