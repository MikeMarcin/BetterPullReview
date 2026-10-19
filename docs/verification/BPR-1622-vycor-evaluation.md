# BPR-1622: stock vycor-cpp evaluation

2026-10-18; mandate `vycor-evaluation-r1`; milestone
`vycor-evaluation-v1/stock-tool-evaluation`.

**Result:** stock megascope exposes the semantic facts needed for the four
small cases on macOS and WSL2. It also parses the selected real BPR sources.
Keep it available for offline investigation; do not replace an architecture
gate with it yet. Stock anneal has no external-determinism/effect policy,
native Windows is blocked, macro expansion provenance is incomplete, and
index process success does not establish complete parsing.

No custom checker, extension, upstream patch, product dependency or gate was
added. The [external-determinism contract](../architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism)
and [BPR-1621's textual-enforcement limitations](../issues/_checkpoints/BPR-1621/audit.md#limits-and-self-review)
remain authoritative. This is an evaluation outcome, not adoption approval.

## Candidate and tools

- BPR fixture/runner candidate: `d2c557f16f48340e34075fbfd717d4703fd1791c`
  (initial probes `437995da0`, explicit complete-coverage refinement `d2c557f16`).
- Upstream: [bearbones/vycor-cpp](https://github.com/bearbones/vycor-cpp), unmodified
  commit `60ef56f80594f40aea8bf5ad7ee4a4e3a09bb0a6`, reporting version 0.2.0.
  Licence: Apache-2.0. Both source checkouts remained clean.
- Supported embedded LLVM versions at that pin: 18, 20, 21. LLVM 22+ is
  explicitly unsupported in `COMPATIBILITY.md` and refused by stock CMake.
  BPR's clang-format/clang-tidy 22 pin is unchanged.
- Mac: arm64, AppleClang 21.0.0.21000101 host compiler, Homebrew LLVM 21.1.8
  libraries. The `llvm@21` 21.1.8 arm64 Tahoe bottle was fetched and extracted
  privately; it was not installed, linked or selected globally.
- WSL2: observed Ubuntu 26.04 LTS, x86_64,
  `6.18.33.2-microsoft-standard-WSL2`; Ubuntu Clang/LLVM 21.1.8
  (`1:21.1.8-6ubuntu1`). Runtime/compiler were installed; development archives
  were missing. Matching `llvm-21`, `llvm-21-dev`, `llvm-21-tools`,
  `llvm-21-linker-tools`, `libclang-21-dev` and `libclang-cpp21-dev` packages
  were downloaded and extracted under an owned temporary SDK, with symlinks
  to installed matching runtime libraries and compiler executables. Installed
  Clang 21 CMake metadata was copied into that SDK. No system package changed.
- Native Windows: Windows 11 Home 10.0.26200; MSVC 19.51.36260.0,
  Windows SDK 10.0.26100.0; installed Clang 22.1.8. See blockers below.

Both successful builds use stock CMake with `VYCOR_BUILD_TESTS=OFF`,
`VYCOR_USE_SUBMODULE=OFF`, explicit `LLVM_DIR` and `Clang_DIR`, Release mode,
and `cmake --build ... --target vycor-cpp -j 4`. No LLVM submodule was built.
Upstream's test suite was not run; the following direct public CLI probes
are the acceptance surface for this evaluation.

## Four cases and controls

The [five translation units](../../tests/fixtures/vycor-evaluation/) are
analysis input only: they are neither linked nor executed. The
[capture runner](../../scripts/evaluation/vycor_probe.py) builds a scratch
compilation database, invokes only stock commands and saves responses and
command receipts. It does not classify BPR source effects.

Both macOS/libc++ and WSL2/libstdc++ report requested=5, indexed=5,
partial=0, failed=0. Each dump contains 16 call-site contexts. Stock
`anneal` defaults analyze 5/5 with **zero findings** on each platform.

| Case | Stock observation on both platforms | Capability and limit |
|---|---|---|
| `is_directory(path)` versus `is_directory(file_status)` | `dump` has different `calleeUsr` values for `path_effect` and `status_value`; `lookup-function --usr` reports their distinct standard-header declaration locations. | Resolved overload distinction available. `get-callees` display names alone are identical and insufficient. Name-only `get-callers` returns `ambiguous`, exit 4. |
| Alias/macro real clock versus unrelated `now()` | `aliased_clock` and `macro_clock` resolve to `std::chrono::system_clock::now`; `supplied_value` resolves to `probe::domain_clock::now`, with a different USR/source declaration. | Actual callee identity survives the header alias and macro. The macro call-site points to `capabilities.hpp:10:30`, its spelling, rather than the expansion at the use site; stock output provides no alias or macro expansion chain. |
| Worker creation versus harmless references | `spawn_worker` has a `ConstructorCall` plus a proven `ThreadEntry` to `worker_body`, with `executionContext: ThreadSpawn`. `thread_reference` has zero callees. Default and moved handles have constructor edges with different USRs but no `ThreadEntry`. | `list-concurrency-entry-points` returns exactly one entry. This distinguishes these controls; it does not prove all indirect callable/lifecycle forms are covered. Spawner recognition is name-based in upstream. |
| Cross-file filesystem wrapper | `find-call-chain --from probe::cross_file_effect --to-usr <resolved path overload>` returns one exhaustive two-edge path: `entry.cpp` → `probe::wrapper` in `wrapper.cpp` → path overload. | Cross-TU direct path available. The parallel value wrapper reaches only the file_status overload; asking it for a path to the path overload returns zero paths (exit 1). |

USR examples from WSL are
`c:@N@std@N@filesystem@F@is_directory#$@N@std@N@filesystem@S@file_status#`
and
`c:@N@std@N@filesystem@F@is_directory#&1$@N@std@N@filesystem@N@__cxx11@S@path#`.
Mac USRs include libc++'s `__1`/`__fs` namespaces instead. A future policy
must bind target declarations/types, not hard-code one platform's USR string.
The queries above use identities returned by the current index.

Additional falsification controls:

- An unknown function returns `not_found`, exit 1. It is not an empty proven
  caller set.
- Indexing only `entry.cpp`, omitting `wrapper.cpp`, removes the filesystem
  path. Its `indexScope.complete` is nevertheless true **for the one requested
  TU**. This is why the requested scope must be checked against the intended
  source inventory; complete parsing of an incomplete selection is not a
  whole-project proof.
- A deliberately missing include yields indexed=0, partial=1, failed=0,
  `complete:false`, with file status `partial`. **`megascope index` still
  exits 0.** The runner explicitly requires the main probe's requested and
  indexed counts to equal five and partial/failed counts to equal zero.
- An initial raw WSL BPR command printed errors for unsupported GCC flags
  while the tool still reported indexed=1 and exited 0. That run is not
  counted as clean evidence; the documented derived database was used below.

## Real BPR compilation databases

Mac source candidate before evaluation-only additions:
`ef7fa9cc6` (full source unchanged through claim revision
`f1f68bb86a68cdca5e44dbfb44a31b4cac81fff1`). WSL checkout:
`d16ae68fd7134e9dc6238c1c7a79af46e011caaf`, clean when inspected. The WSL
checkout is older; its results do not qualify the current Mac source revision.
Neither checkout was rebuilt or reconfigured for these parses.

| Database and selected scope | Requested / indexed / partial / failed | Cold bake | Reported peak RSS |
|---|---|---:|---:|
| Mac `build/core-only`: every unique first-party `src/core/` file | 136 / 136 / 0 / 0 | 65.45 s | 155,024 KiB, parent only |
| Mac `build/dev`: seven representative files below | 7 / 7 / 0 / 0 | 25.71 s | 1,270,160 KiB, in-process run |
| WSL `build/core-only`: every first-party `src/core/` file | 107 / 107 / 0 / 0 | 80.49 s | 148,432 KiB, parent only |
| WSL `build/dev`: six representative files below | 6 / 6 / 0 / 0 | 25.13 s | 170,616 KiB, parent only |

All four final runs report parse_errors=0 and crashes=0; stderr has no
compiler errors. RSS comes from upstream's `RUSAGE_SELF` statistics and
**excludes isolated worker memory**, so the small parent numbers are not a
total-memory budget. Core and WSL representative runs use two isolated
workers, a 4096 MiB per-worker limit and 120-second worker timeout. Mac's
representative run uses one in-process thread. Times are observations of
these machines, with other work present, not performance guarantees.

Mac databases contain 717 and 1271 total entries respectively. Each has
137 core entries but 136 unique core files: `sha256_hacl.cpp` has two
different commands (ordinary core definitions versus an experimental-libc++
test variant). Selection is by unique source path and upstream coverage is
per file; **137 independent command variants are not certified**. This was
not a full 717/1271-entry index. WSL databases contain 445 and 786 entries,
with 107 and 108 core entries respectively; the 107-file core-only graph
was selected. Tests, third-party source and the remaining app TUs were not
indexed.

Representative files on both platforms:
`src/adapters/qt/src/workbench_window.cpp`,
`src/adapters/workspace/src/local_workspace_source.cpp`,
`src/app/src/foundation_application.cpp`,
`src/app/src/platform_credential_vault.cpp`, and
`src/application/src/github_application_host.cpp`.
Mac additionally selects `src/app/main.cpp` and
`src/core/src/sha256_commoncrypto.cpp`; WSL additionally selects
`src/app/src/profile_desktop.cpp`.

A stock query over Mac's actual representative index finds one proven
`openat` caller, `bpr::adapters::workspace::(anonymous namespace)::open_at`,
at `local_workspace_source.cpp:1044:24`, resolving to `c:@F@openat` from
the SDK. This confirms useful facts beyond fixture names. No source-wide
effect verdict is claimed from these selected graphs.

Original database SHA-256 identities:

| Host | `core-only/compile_commands.json` | `dev/compile_commands.json` |
|---|---|---|
| Mac | `94caa6727c220f0283b16b31a4330233bc8654841da0d43c052f9e4c189333b1` | `624d8e1db704132f294e44e52c910ac4dc9420951e11c695eb2dd803bcb10c47` |
| WSL | `b609ae894ef26b5d3f0cb599be1b66b93d0c631de31aaed7541a23c4ddea6df6` | `638adb323cca465db05fea06ad99308a13ad53438409edd6bf0ba0614a3e27ff` |

### Every adaptation

Mac database bytes and flags were left unchanged. The privately unpacked
Homebrew bottle retains install-name placeholders, so launching the tool
requires `DYLD_LIBRARY_PATH` naming its private `lib` directory and the
installed zstd library directory. Resource-header discovery failed at CMake
configure for that unpacked compiler; `VYCOR_EXTRA_ARGS=-resource-dir=<private
LLVM root>/lib/clang/21` supplied the matching headers to every parse.
These are explicit tool-environment adaptations, not BPR compiler changes.
Without them the tool first failed at dynamic loading, then failed all
136 core and seven representative parses (missing `stdarg.h`, SDK type
errors and recorded crashes). Those initial attempts are not passing evidence.

WSL originals were read without modification. A temporary derived copy converts
the shell command to an argument array with Python `shlex.split`, removes
`-fmodules-ts`, `-fmodule-mapper=...`, `-fdeps-format=p1689r5`, and replaces
`-Wshadow=local` with `-Wshadow`. These are GCC module/dependency-generation
and diagnostic options unsupported by the embedded Clang. Include paths,
defines, language standard, working directory and source paths are retained.
The transformation touches 444/445 core-only and 785/786 dev database entries;
only the stated source selections were parsed. A per-entry adaptation receipt
is retained in runtime evidence. No generated header was fabricated, no file
silently skipped, no standard changed and no BPR compiler was replaced.

Upstream itself also strips selected incompatible warning/module/PCH flags,
including `-Werror`, and supplies resource/sysroot/toolchain adjustments
(`include/vycor/compat/ToolAdjusters.h`). Thus successful analysis is not a
reproduction of BPR's compiler warning gate.

## Portability verdict

**WSL2 Ubuntu: demonstrated for this Linux use.** Stock tool builds, isolated
workers run, indexes save/reload, and both probes and actual Linux compilation
commands parse. The development SDK must match supported LLVM. This does not
exercise Windows preprocessor branches, the Windows SDK or an MSVC database.

**Native Windows: unsupported for adoption at this pin.** Stock CMake under
VS 2026 identifies MSVC/SDK but fails because no supported LLVM/Clang
development package is present. The installed LLVM 22 compiler is outside
the declared support set. Independently of that environment gap,
`Interrupt.cpp` unconditionally includes `pthread.h`, `signal.h`, `unistd.h`
and uses `sigwait`, `pthread_sigmask`, `sigaction`, `setenv`, POSIX process
IDs and `kill`. `WorkerPool.cpp` unconditionally includes `sys/wait.h` and
uses `waitpid`, `WNOHANG`, `SIGKILL` and `kill`. These files are always compiled
into `vycor_lib`; one-thread mode cannot avoid the build barrier. Actual
native `clang++ -fsyntax-only` probes for `pthread.h`, `unistd.h` and
`sys/wait.h` each fail with header-not-found, exit 1. No native vycor binary
or native BPR parse was produced, and none is counted as passing.

There is some Windows-aware code: MSVC flags in CMake and LLVM file-lock/
descriptor fallbacks in `AtomicFile.cpp`. It does not cover the unconditional
worker/interrupt code. Its Windows atomic-write path also omits the POSIX
fsync/directory-sync steps. Release documentation names Linux x86_64 and
macOS arm64, with matching dynamic LLVM runtimes, not a Windows package.

Snapshot encoding uses explicit little-endian integer writes and bounded
reads rather than native struct dumps; the observed index format is 14.
That is encouraging for file-format portability, not a tested cross-platform
index interchange promise. Absolute source paths, environment fingerprints,
stdlib USRs and native command semantics still bind an index to its build
environment. Reindex on the intended platform. No Windows port was attempted.

## Reproduction and next decision

Use the pinned upstream source, supported LLVM development files and the
stock CMake options above, then run from an explicit repository root:

```sh
python3 /absolute/BPR/scripts/evaluation/vycor_probe.py \
  --tool /absolute/vycor-build/src/vycor-cpp \
  --clang /absolute/compatible-clang++ \
  --output /absolute/new-probe-output
```

On an unpacked Mac SDK supply the two documented environment variables.
Choose a fresh output directory. Inspect `commands.json`, `info.stdout`,
`dump.stdout`, the identity/chain query responses, `anneal.stdout` and stderr.
Expected controls deliberately return 1 or 4; the broken-parse control
deliberately records incomplete coverage. Runtime evidence under
`/tmp/bpr-vycor-evaluation/` retains the exact invocations, errors, stats and
per-entry WSL adaptations; it is not a repository dependency.

For real BPR scope use `megascope index --build-path <actual-or-derived-db>`
with an explicit `--source-list`, separate `--index`, `--stats-json`, and the
bounded worker options above; query `megascope info --files` afterward.
Compare requested paths and all parse states with that list, including
multiple compile variants. Do not use an exit code or `complete:true` alone.

The tool supplies resolved compiler facts, so a new C++ parser is unnecessary
for these distinctions. A BPR rule still needs a reviewed effect/API model,
source ownership, unknown-call handling and complete selection/parse checks.
Upstream offers per-TU `AnnealCheck` and cross-TU `IndexCheck` extension points;
none was implemented here. A future custom clang-tidy 22 check would fit
BPR's existing native toolchain and single-TU overload/constructor/macro
diagnostics, but a single-TU check does not by itself follow a definition in
another `.cpp`. That requires an explicit cross-TU summary/analysis design.
Choose between an upstream Windows fix plus a vycor extension and a native
clang-tidy-based approach only after deciding that requirement. Existing
architecture guards continue unchanged in the meantime.

## Verification and closeout

The exact final capture runner was exercised on both platforms. Self-review
checked source/callee identity, false-success controls, requested coverage,
macro-source limitations, native branches, memory-stat scope and explicit
argument adaptations. No mandatory human or independent acceptance applies to
this evaluate-only milestone; native Windows adoption remains unsupported.

Focused checks: six fixture files pass clang-format **22.1.8**; Python runner
compiles and its public CLI completes; `git diff --check` passes;
`python3 scripts/verify_documentation.py --root .` passes;
`python3 tests/scripts/verify_documentation_tests.py` passes all 67 tests.
No product rebuild, full product test matrix or gate replacement was needed.
Product, security, persistence, UI and release behavior have no impact.
