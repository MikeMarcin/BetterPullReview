---
schema: "implement-rfc.card.v1"
id: "BPR-0109"
title: "Acquire KSyntaxHighlighting as a verified build dependency"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0103"]
---

# Acquire KSyntaxHighlighting as a verified build dependency

Add KSyntaxHighlighting to the build under the existing dependency-boundary rules: pinned version, cryptographic digest, offline-capable cache, and a recorded provenance entry.

Caller-installed discovery is not available on this platform — no Homebrew formula exists for it or for the KDE Frameworks — so CPM source acquisition is the only route that satisfies the RFC's mandate to use the library at all. That pulls extra-cmake-modules as a transitive build dependency, which the dependency inventory must record.

Production behaviour is unchanged by acquisition: definitions and approved themes are packaged as resources, and standard-path discovery and downloads stay disabled in the production build, per the RFC's non-goals.

## Outcome

It works. KSyntaxHighlighting 6.28.1 and extra-cmake-modules 6.28.0 are
acquired as verified CPM source, both pinned by version and by two digests, and
`libKF6SyntaxHighlighting.a` builds from the verified tree.

This slice was framed as one that might legitimately fail, because
KSyntaxHighlighting is a full KDE Frameworks CMake project rather than the
single translation unit the SQLite precedent handles. It did not fail, and none
of the shortcuts that would have manufactured a green were taken: no gate was
weakened, no warning suppressed, nothing vendored or stubbed.

## Provenance

Each input carries an archive digest and an extracted-source digest. The second
is computed over a sorted manifest of every file's own digest, so it detects a
changed file, an added file, and a removed one — not merely a tampered archive.
The provider rejects a symbolic link anywhere in the extracted tree rather than
following it.

extra-cmake-modules is recorded with the same rigour as the library itself. It
is a transitive build input rather than a product dependency, but an unpinned
build input is as much a supply-chain surface as an unpinned library, so it
gets its own pin, digest, and provenance entry.

KSyntaxHighlighting 6.28.1 requires caller-installed Qt 6.9 or newer, above the
BetterPullReview floor. The provider states that minimum itself and fails
naming both versions, rather than letting the nested configure fail somewhere
less legible.

## Direct verification

- `cmake --preset dev` and `cmake --build --preset dev` succeed, and
  `libKF6SyntaxHighlighting.a` exists in the build tree — the library genuinely
  builds rather than being downloaded and skipped.
- **The Qt-free `core-only` preset does not acquire it.** Its cache contains
  zero references to the dependency, and it configures, builds, and passes
  38/38 without Qt. This was the constraint most likely to be quietly
  sacrificed, so it was checked directly rather than inferred.
- `cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON` succeeds from cache with no
  download attempted, so a first fetch is the only network need.
- `architecture_boundaries`, `architecture_boundary_contract`, and
  `dependency_modernization_contract` pass 3/3.

## Corrected during integration

The provenance document was an explicit deliverable and had not been written,
so it was added here rather than left for a later slice to discover. A pinned
digest that exists only in a CMake file is a build detail; a pinned digest with
a provenance record is an auditable supply-chain claim, and the difference
matters precisely when someone later asks where a dependency came from.

## Result

The dependency the RFC mandates is available and verified, the Qt-free preset
is provably unaffected, and offline configures work from cache. No product
target links it yet — that is BPR-0110. BPR-0109 is complete.
