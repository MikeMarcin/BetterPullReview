---
schema: "implement-rfc.card.v1"
id: "BPR-1832"
title: "Share one compiler cache across worktrees and presets"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1832--BPR-1832--1d5847578b40.json","kind":"tooling","found_by":"owner: build folders filled the disk, and every agent worktree (.claude/worktrees/*, ~/.codex/worktrees/*) compiled everything from scratch","tool":"ccache 4.14.2 (Homebrew), GPL-3.0-or-later, a build tool that is never linked or shipped"}
acceptance: [{"criterion":"AC1: an optional, auto-detected launcher with an opt-out; the build is unchanged without the tool","outcome":"pass","revision":"e31add119","evidence":{"kind":"command","command":"cmake --preset core-only -DBPR_CCACHE_PROGRAM=/nonexistent/ccache; cmake --preset core-only -DBPR_COMPILER_CACHE=OFF; cmake --preset core-only -DBPR_COMPILER_CACHE=AUTO; ctest -R '^compiler_cache_contract$' after each","detail":"Absent: 'Compiler cache: off (skipped: ccache was not found on PATH)', 0 ccache occurrences in build.ninja. OFF: 'skipped: BPR_COMPILER_CACHE=OFF', 0. AUTO with ccache: enabled, 795 launcher occurrences. compiler_cache_contract passed for all three graphs and in build/dev."}},{"criterion":"AC2: release, qualification and signing builds unchanged","outcome":"pass","revision":"e31add119","evidence":{"kind":"command","command":"cmake --preset macos-release -B build/macos-release-check; cmake --preset macos-release -B build/macos-release-check -DBPR_COMPILER_CACHE=ON","detail":"Distribution: 'off (skipped: a distribution graph never compiles through a cache)', no ccache or -fdebug-prefix-map in build.ninja or compile_commands.json; ON is a configure error. AUTO skips Release graphs (qualification-release, app-release, tour-cli); the decision table covers both. CMakePresets.json is untouched, so release configuration inputs keep their digests."}},{"criterion":"AC3: hits across worktrees, measured","outcome":"pass","revision":"e31add119","evidence":{"kind":"observation","detail":"See Measurements below: 0/622 hits cold in worktree A (224.8 s), 618/622 (99.4%) in a fresh worktree B (12.6 s) at -g0; 327/327 at the default -g (43.6 s to 2.3 s); 590/594 with a shared CPM cache."}},{"criterion":"AC4: repo-local config, cache outside every worktree, size cap, install documented","outcome":"pass","revision":"e31add119","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"cmake/bpr-ccache.conf (max_size = 20G); default cache ~/Library/Caches/bpr-compiler-cache or ${XDG_CACHE_HOME:-~/.cache}/bpr-compiler-cache; docs/development/build-and-test.md 'Shared compiler cache' and its Prerequisites entry. DOCUMENTATION_OK."}},{"criterion":"AC5: gates","outcome":"pass","revision":"e31add119","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; ctest --test-dir build/dev -R '^(compiler_cache_contract|macos_keychain_native.*|architecture_boundaries|architecture_boundary_contract|core_boundary_contract|msvc_secure_crt.*)$'","detail":"clang-format 22.1.8 over 1,871 files passes. The listed rows pass, the macOS keychain native rows built through ccache. test_network_isolation_manifest fails on the same two rows it fails on main (application_agent_chat_custom_agent_contract, gh_cli_git_smart_transport), which this card does not touch."}}]
---

# BPR-1832: Share one compiler cache across worktrees and presets

Agents build in many worktrees at once, and each worktree compiled the whole
graph from nothing, while its `build/` filled the disk. This card adds an
optional, shared ccache.

## Acceptance

- **AC1:** an optional, auto-detected compiler launcher with an opt-out
  (`-DBPR_COMPILER_CACHE=OFF`). Without the tool the build is unchanged.
- **AC2:** release, qualification and signing builds are left as they were.
- **AC3:** cache hits survive a move to another worktree, measured.
- **AC4:** a repo-local config, a size cap, a cache outside every worktree,
  and the one-time install documented.
- **AC5:** clang-format 22, focused tests, documentation gate.

## Why ccache, not sccache

| | ccache 4.14 | sccache |
|---|---|---|
| Licence | GPL-3.0-or-later. It is only a build tool: never linked, never shipped | Apache-2.0 |
| macOS, Linux | Yes | Yes |
| Windows/MSVC | Yes, with Ninja and `/Z7`; `/Zi` program databases cannot be cached | Yes, with the same `/Z7` limit |
| Hits from another directory | `base_dir` rewrites paths inside a root to relative ones; `-fdebug-prefix-map` is not hashed (both measured below) | Hashes the compile arguments with their absolute paths; no `base_dir` equivalent to rely on |
| Lookup | Direct mode: hashes sources and headers without running the preprocessor | Runs the preprocessor on every lookup |
| Precompiled headers, C++20 modules | PCH with `pch_defines,time_macros` sloppiness; a modules unit compiles uncached, never wrongly. The graph uses neither today | Weaker PCH support; not needed here either |
| Process model | One process per compile, a size-capped local directory, safe for concurrent builds | A background server that builds start and must stop |

Cross-worktree hits were the point, so ccache wins on `base_dir` alone. It is
also on every package manager the project's hosts use (Homebrew 4.14.2,
Debian 12 4.8, Ubuntu 24.04 4.9).

## Design

- **Module.** `cmake/BprCompilerCache.cmake`, included from the root
  `CMakeLists.txt` before any target, so dependencies compile through it too.
  `bpr_compiler_cache_decision` decides from explicit inputs only, which
  lets `tests/cmake/compiler_cache_tests.cmake` drive every branch in script
  mode.
- **Policy.** `BPR_COMPILER_CACHE` is `AUTO` (default), `ON` or `OFF`. `AUTO`
  uses ccache 4.8 or newer when it is on `PATH` and the graph is a Debug,
  non-distribution Ninja or Makefile graph built by Clang or GCC. `ON`
  requires it for any non-distribution graph. A distribution graph never uses
  it, and `ON` there is a configure error. An explicit
  `CMAKE_<LANG>_COMPILER_LAUNCHER` is left alone. Visual Studio and Xcode
  generators ignore launchers, and MSVC is skipped.
- **Settings.** ccache 4.8 reads `key=value` arguments before the compiler,
  so the launcher is `ccache cache_dir=… base_dir=<checkout> max_size=20G`
  (plus `file_clone=true compression=false` on macOS) and no environment
  wrapper is needed. `cmake/bpr-ccache.conf` holds the shipped settings.
  Configure also writes them to `<cache>/ccache.conf`, because
  `CCACHE_DIR=<cache> ccache -s` or `-c` reads only that file and would
  otherwise evict to ccache's default 5 GB.
- **Cross-worktree paths.** `base_dir` is the checkout's own source directory.
  Every path inside it, including `build/.cpm-cache`, reaches the compiler
  relative to the build directory, and that relative path is the same in every
  worktree. `-fdebug-prefix-map=<build>=.` makes the DWARF compile directory
  `.`. ccache does not hash that option, so `-g` builds hit across worktrees
  too (`DW_AT_name ../../src/core/src/diff_compute.cpp`,
  `DW_AT_comp_dir .`).
- **APFS.** With `file_clone`, a hit is a clone of the cached object, so
  worktrees share one copy on disk. `du` still counts each clone in full.

## Consequences found and handled

- `__FILE__` becomes relative (`../../tests/…`). Two native credential audits
  scanned their own source through `__FILE__`. They now take it from
  `BPR_NATIVE_TEST_SOURCE_FILE`. The macOS rows pass when built through
  ccache. The Windows audit is not compiled through ccache and is left as it
  was. The two fixture-root fallbacks that read `__FILE__`
  (`github_provider_tests.cpp`, `github_capability_probe_tests.cpp`) are not
  compiled: both units get `BPR_GITHUB_CAPABILITY_FIXTURE_ROOT`.
- A debugger finds sources when started from the build directory. Started
  elsewhere, LLDB needs `settings set target.source-map ../.. <checkout>`
  (checked with a probe binary). `.vscode/launch.json`'s `cwd` is the
  workspace, which this card does not change.
- `compile_commands.json` has no launcher (clangd and clang-tidy see the
  compiler), but it does carry `-fdebug-prefix-map`.

## Measurements

The host was macOS arm64 with AppleClang, Ninja and ccache 4.14.2, installed
by this card with `brew install ccache`. Worktree A was this worktree, and B a
fresh `git worktree add` of the same commit with the change applied. Both used
the `dev` preset and the targets `better_pull_review_core`,
`better_pull_review_application`, `better_pull_review_adapter_github` and
`better_pull_review_adapter_qt` (622 compiled units). Other agents were
building at the same time, so the times are indicative.

| Run | Hits | Wall time | Disk |
|---|---|---|---|
| A, cold, `-g0` | 0 / 622 | 224.8 s | `build/dev` 831 MB; cache 0.4 GB |
| B, fresh worktree, `-g0` | 618 / 622 (99.4%) | 12.6 s | `build/dev` 819 MB by `du`; its 666 objects (245.8 MB) are APFS clones of cache entries; cache unchanged at 0.4 GB |
| A, `better_pull_review_core`, default `-g` | 0 / 327 | 43.6 s | |
| B, same | 327 / 327 (100%) | 2.3 s | |
| A, new tree, shared `CPM_SOURCE_CACHE`, `adapter_qt` and its dependencies, `-g` | 456 / 594 | 101.0 s | the 138 misses are the Qt-including units, whose Qt paths moved |
| B, new tree, same shared `CPM_SOURCE_CACHE` | 590 / 594 (99.3%) | 13.4 s | |

The four misses each time are the Qt resource units
(`qrc_bpr_{dif,code_font,about,status_item}.cpp`). rcc records file
modification times, which differ between checkouts.

**Dependency cache.** Each worktree's `build/.cpm-cache` is 1.9 GB, 1.7 GB of
it Qt; three other agent worktrees each held one. Configure took 196 s with
the Qt download and 99 s with the cache already present. Sharing
`CPM_SOURCE_CACHE` outside the checkouts works and keeps hits, as the last two
rows show, but only between trees that all use it. Switching an existing tree
fails with `Unknown CMake command "CPMAddPackage"`, so it needs a new build
tree. The presets keep `build/.cpm-cache`: `CMakePresets.json` is a release
configuration input, and the package records its `CPM_SOURCE_CACHE`. The
build guide documents the opt-in.

After the measurements, worktree B, the shared CPM clone and the extra build
trees were removed. The shipped cache directory
`~/Library/Caches/bpr-compiler-cache` stays, holding the measured entries.

## Left out

- **Windows.** `windows-dev` uses the Visual Studio generator, which ignores
  launchers, and MSVC's `/Zi` cannot be cached. It needs a Ninja development
  preset with `/Z7`, measured on the Windows station.
- **Release development graphs** (`qualification-release`, `app-release`,
  `tour-cli`) need `-DBPR_COMPILER_CACHE=ON`. AUTO leaves them alone, as this
  card was asked to.
- **A shared CPM cache by default.** That would change `CMakePresets.json`
  and the release package inputs, so it stays a documented opt-in.
