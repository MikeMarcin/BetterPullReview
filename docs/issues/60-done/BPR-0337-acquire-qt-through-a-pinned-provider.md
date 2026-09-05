---
schema: "implement-rfc.card.v1"
id: "BPR-0337"
title: "Acquire Qt through a pinned provider"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0336"]
---

# Acquire Qt through a pinned provider

Until now the dependency boundary required the caller to install Qt, and
`BprDependencies.cmake` failed with `Qt is never fetched automatically` when
they had not. That was workable on a host someone had already set up by hand. It
is the wrong shape for the platform work [[BPR-0036]] and [[BPR-0156]] describe,
where the whole point is that a new target machine must be able to build from a
clean checkout.

Owner direction, 2026-08-17: Qt should be acquired by the build, in the shape
the `containers` project gets Qt from vcpkg, using CPM.

## What was built

`cmake/BprQtProvider.cmake`, mirroring `BprSQLiteProvider`: one pinned version,
a shared cache under `CPM_SOURCE_CACHE`, a lock around acquisition, an offline
mode, and validation of what was obtained.

Qt differs from SQLite in one respect that shaped the design. It is a binary
dependency — upstream publishes prebuilt packages but no source archive this
project could verify and build in reasonable time — so the provider retrieves
official prebuilt packages rather than source. Retrieval uses a pinned
`aqtinstall` in a private virtual environment, so acquiring Qt never mutates
whichever interpreter the developer happens to have.

Discovery is unchanged. The provider appends a prefix to `CMAKE_PREFIX_PATH`
and the existing `find_package(Qt6 CONFIG COMPONENTS Core Gui Widgets Network)`
resolves it, so nothing downstream depends on how Qt arrived.

| Knob | Effect |
|---|---|
| *(default)* | Acquire the pinned Qt into the dependency cache |
| `BPR_QT_ROOT=<prefix>` | Use an existing prefix, validated the same way |
| `BPR_QT_PROVIDER=OFF` | Restore the former caller-installed requirement |

The `BPR_BUILD_QT=OFF` graph never calls the provider, so `core-only` still
needs no Qt at all — the same feature-gating the `containers` manifest gets
from `VCPKG_MANIFEST_FEATURES`.

## Pin

Qt **6.10.3**, `win64_msvc2022_64` / `linux_gcc_64` / `clang_64`.

6.11.1 was the first choice, being the version the macOS evidence already used,
but `aqtinstall` cannot resolve any 6.11.x — the repository mapping returns no
architectures. 6.10.3 is the newest version it can retrieve, sits one patch
above the Qt 6.10.2 the Linux evidence used, and clears both the project floor
of 6.8 and the KSyntaxHighlighting floor of 6.9. The provider asserts the pin
against `BPR_QT_MINIMUM_VERSION` so a future re-pin cannot silently drop below
it.

## Two failure modes found while building this

**`aqtinstall` reports success while writing an empty prefix.** Observed twice
against different versions, intermittently. Exit status is therefore not
evidence: each attempt is judged by revalidating the prefix, a failed attempt is
retried up to three times, and a partial prefix is removed so the next configure
cannot mistake it for a cache hit.

**Qt tools built outside Qt's own `bin` cannot find Qt at runtime on Windows.**
The KSyntaxHighlighting indexer runs during configure and exited with
`0xc0000135` (`STATUS_DLL_NOT_FOUND`), because Windows has no rpath equivalent
and the tool is not adjacent to the DLLs. `bpr_expose_qt_runtime` prepends Qt's
`bin` to the configure-time `PATH`. Qt's own `moc`/`rcc` were never affected —
they sit beside the DLLs and resolve through the executable's own directory.

## Evidence

From a clean `build/dev`, with Qt absent from `PATH` and no Qt installed
anywhere the build was told about:

- `cmake --preset dev` acquires Qt 6.10.3 (~2.1 GB, ~32 s) and configures.
- `Qt6_DIR` resolves to
  `build/.cpm-cache/bprqt/6.10.3/6.10.3/msvc2022_64/lib/cmake/Qt6`, confirming
  the provider's copy is the one in use.
- KSyntaxHighlighting 6.28.1 builds against it under MSVC 14.51 and its Qt 6.9
  floor check passes.
- The full graph generates: 100 non-production executables registered.

The prebuilt packages are built with MSVC 2022 and are consumed here by MSVC
14.51 from Visual Studio 2026; compiling and linking KSyntaxHighlighting against
them exercised that combination and it holds.

## Documentation

The dependency boundary previously asserted in eight places that Qt is never
fetched. Live statements in `README.md`, `docs/development/build-and-test.md`,
`docs/architecture/system-architecture.md`, and
`third_party/ksyntaxhighlighting/PROVENANCE.md` are corrected. The two
verification ledgers, `RFC-BPR-0004.md` and `RFC-BPR-0006.md`, record what was
true when those RFCs were verified, so they keep their original text and carry a
supersession note instead. Neither RFC's reasoning changes: KSyntaxHighlighting
went to CPM because no Homebrew formula exists, not because of how Qt arrived.

## What this card does not claim

- No Linux or macOS acquisition has been exercised. The architecture mapping for
  both is written and neither has been run; only Windows is proven.
- No Windows runtime property is proven. The Qt graph builds, but every test
  still fails at launch on the boundary [[BPR-0235]] owns.
- No packaging or redistribution claim. How Qt is obtained for a *build* says
  nothing about how it would be shipped; RFC-BPR-0014 owns that.
