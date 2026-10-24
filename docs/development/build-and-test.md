# Build and Test

This guide takes you from a fresh clone to a built application and a passing
test run. It covers prerequisites, the first build, the presets, running tests
and the coding checks every change must pass.

The long-form material (dependency providers, per-library notes, network
isolation, attended and live courses, fuzzing and evaluation harnesses) lives in
the [build and test reference](build-and-test-reference.md). See
[Reference topics](#reference-topics) at the end of this page for a map.

Builds, tests and releases run on your own machine or on the target platform.
GitHub Actions does not configure, build, test, package, release or verify
BetterPullReview.

## Prerequisites

Install these before configuring:

- CMake 4.4.2
- Ninja
- A C++23 compiler
- Python 3.10 or newer, for the verification scripts. On macOS,
  `/usr/bin/python3` is too old; make sure `python3` resolves to a newer
  interpreter (for example Homebrew's)
- Git 2.39 or newer, for the installed-Git contract tests
- clang-format and clang-tidy 22.1.8, for the coding checks. The format gate
  accepts clang-format 22 and no other major release (`brew install llvm@22`
  on macOS, `apt install clang-format-22` on Debian and Ubuntu); see
  [Format with clang-format 22](#format-with-clang-format-22)

**Qt.** You do not need to install Qt yourself. Configure acquires a pinned
Qt 6.12.0 into the shared dependency cache. To use a Qt you already have, pass
`-DBPR_QT_ROOT=<prefix>` (an official 6.12.0 prefix, which also works offline),
or pass `-DBPR_QT_PROVIDER=OFF` to require a caller-installed Qt 6.8 or newer
including CoreTools (`rcc`). On Windows, Qt acquisition needs Git on `PATH`.

**Linux only:**

- Stable `libsecret-1>=0.20.0` and its development metadata
- libxcb's development metadata (`xcb` in `pkg-config`), for the application's
  X11 activation host
- `dbus-daemon`, `dbus-send` and `gnome-keyring-daemon` in `/usr/bin` or
  `/bin` (on Ubuntu, the `dbus-daemon`, `dbus-bin` and `gnome-keyring`
  packages), for the native Secret Service test rows. Each of those rows starts
  its own private bus and test-only keyring and never uses your desktop
  session. Without the tools, see [Troubleshooting](#troubleshooting)

The validated local environment is CMake 4.4.2, AppleClang 16, Ninja,
Python 3.14 and Qt 6.11.1 on macOS arm64.

## Product allocator

Normal desktop and standalone `bpr` builds select the pinned mimalloc 3.5.3
shared runtime for each selected shipped BPR executable, including helpers.
Configure verifies the archive and MIT licence; offline mode requires
`mimalloc-3.5.3.tar.gz` in `CPM_SOURCE_CACHE`, and rejects modified bytes.
A core-only graph acquires no mimalloc. For matched development comparisons,
configure a separate build tree with `-DBPR_USE_MIMALLOC=OFF`; distribution
refuses that baseline. Sanitizer/fuzzer builds retain their instrumentation
allocator and exclude the override. Do not set `DYLD_INSERT_LIBRARIES` or
`LD_PRELOAD` on a shell or an external child to enable the BPR allocator.

On macOS and native Windows, `ctest --preset dev -R '^mimalloc_' --output-on-failure` exercises
ownership of C/C++, loaded-module and Qt allocations in each selected product
image, cross-module frees and allocation-failure behavior. The probe exists
only with tests enabled and runs before main; ordinary startup/shutdown courses
remain separate. A development pass does not qualify an Intel host or signed
installed package. The [RFC](../rfcs/RFC-BPR-0093-process-wide-mimalloc.md)
retains those required native and package checks.

Windows requires native MSVC x64 with the dynamic CRT; static CRT flags and
selections are refused. The build copies the source-built allocator DLL and
pinned upstream redirect DLL beside each product executable. Distribution
installs both, and its audit requires a full producer map for the allocator
and the independent byte pin for the redirector. Changing only an import
library cannot prove that Qt or loaded-library calls are intercepted.

## First build

Tests use the project’s locked portable tooling. For a fresh clone, register
that owned checkout through MASK using its existing locks, then run the
[actual cheap prerequisite checks](build-and-test-reference.md#complete-suite-timing-and-focused-work)
before an expensive configure or build.

From the repository root:

```sh
cmake --workflow --preset dev
```

The `dev` workflow configures, builds and tests the complete developer graph:
the core, the application host, the Qt adapter, the application and every test.
The build tree is `build/dev`. The first configure downloads pinned
dependencies into `build/.cpm-cache`, which later configures and other presets
reuse.

The same steps, run one at a time:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Then run the application:

| Platform | Executable |
|---|---|
| macOS | `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` |
| Linux | `build/dev/src/app/better_pull_review` |
| Windows (`windows-dev`) | `build/windows-dev/src/app/Debug/better_pull_review.exe` |

On macOS the tests-enabled build is an application bundle named
**BetterPullReview (tests)**, with its own bundle identifier so it never shares
notification permissions with the shipped application.

For clangd, configure `dev` before opening the tree in an editor: the root
`.clangd` points at `build/dev/compile_commands.json`. If you edit against
another preset, change the local `CompileFlags.CompilationDatabase` path to
that preset's build tree (for example `build/windows-dev`) and restore
`build/dev` before committing. Do not commit a compile database.

## Presets

All build trees live under the ignored `build/` directory, one per preset.

| Preset | Use it for | Tests |
|---|---|---|
| `dev` | Everyday development: the complete graph with Qt (Debug) | Yes |
| `core-only` | Proving the core and application host build without Qt (Debug) | Yes |
| `windows-dev` | Development on Windows (Visual Studio 18 2026, multi-config; build and test select Debug) | Yes |
| `tour-cli` | The non-GUI `bpr` terminal tool (Release) | No |
| `app-release` | A tests-disabled Release application for local use | No |
| `macos-release`, `windows-app-release`, `linux-package` | Distribution builds; see the [package guide](release-packages.md) | No |
| `…-agent-free`, `…-agent-free-automation-off` | Distribution builds without agent integrations, or without agent integrations and local automation | No |

Workflow presets (`cmake --workflow --preset <name>`) exist for `dev`,
`core-only`, `app-release`, `macos-release` and `windows-dev`. The `dev`,
`core-only` and `windows-dev` workflows also run the tests.

Ordinary `app-release` builds are not distribution qualification. Distribution
uses `macos-release`, `windows-app-release` or `linux-package`, each Release,
tests disabled and with distribution runtime restrictions; the
[package guide](release-packages.md) covers the required toolchains and the
release procedure.

Three switches shape the graph independently of the preset:

| Switch | Default | What it changes |
|---|---|---|
| `BPR_BUILD_TOUR_CLI` | `OFF` in `dev`, `ON` in `tour-cli` | Builds `bpr` (`app_bpr`) and registers the rows that run it: `cycle_cli_public_path`, `cycle_cli_documentation`, `skill_bpr_cycle_package`, and, in a graph with Qt, the two-round, parity and handoff courses. Requires `BPR_BUILD_LOCAL_AUTOMATION`. |
| `BPR_BUILD_AGENT_INTEGRATIONS` | `ON` (`OFF` in `tour-cli` and the `-agent-free` presets) | Compiles out the agent launch adapter, protocol decoders, agent hosts, MCP helper and Send to Agent. |
| `BPR_BUILD_LOCAL_AUTOMATION` | `ON` (`OFF` in the `-agent-free-automation-off` presets) | Without it no owner command endpoint is bound and `bpr` cannot be built; the automation-off two-round course is registered instead. |

A `dev` graph that also builds `bpr`:

```sh
cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON
```

The [terminal guide](cycle-cli.md) covers `bpr` itself.

### Build targets

| Target | Responsibility |
|---|---|
| `better_pull_review_core` | Qt/effect-free semantic library |
| `better_pull_review_application` | Host, effect/decision ports, serialized completion, manual executor |
| `better_pull_review_adapter_github` | Normalized provider, native-HTTPS, credential-vault, and recorded-test adapters |
| `better_pull_review_adapter_git` | Concrete Qt Core/`QProcess` installed-Git local pack/object/ref adapter |
| `better_pull_review_sqlite3` | Verified CPM-acquired static SQLite 3.53.4 provider, exposed as `BetterPullReview::SQLite3` |
| `better_pull_review_adapter_sqlite` | Concrete BPEC v1 epoch-catalog storage and operation-state adapter |
| `better_pull_review_adapter_ai` | Capability boundary established for a later RFC |
| `better_pull_review_adapter_qt` | Native Qt presentation adapter |
| `better_pull_review_composition` | Explicit profile, transport, vault, provider, worker, host, and Qt wiring |
| `app_better_pull_review` | Native read-only GitHub application target; emits `better_pull_review` |

## Run tests

### Focused tests

For ordinary work, build what you changed and run the affected rows by name:

```sh
ctest --preset dev --output-on-failure -R '^(affected_test_name)$'
```

Add the relevant contract and public-path rows for the surface you touched.
Run the complete suite at an integration or release boundary, not after every
small edit, and reuse unchanged passing results.

### The complete suite

```sh
python3 scripts/run_test_suite.py --preset dev
```

The runner executes every row the preset selects with four CTest jobs, keeping
network isolation, resource locks and fixture dependencies. Timing, native
desktop, global-resource and unclassified courses run alone. Only the explicit
audited Windows correctness allowlist may overlap under its resource locks;
other platforms retain their schedule.

Before starting the full graph, the runner executes its configured cheap
tooling checks. A failed or incomplete preflight leaves the full graph explicitly
not started. Passing preflight still runs every selected row. See the
[policy, bootstrap commands and timing details](build-and-test-reference.md#complete-suite-timing-and-focused-work).

- Use `--jobs 1` for a sequential diagnostic run.
- Avoid other builds and test runs on the machine while measuring
  responsiveness; CTest cannot serialize unrelated processes.
- Each run writes `manifest.json`, `ctest.log`, `junit.xml` and
  `timings.json` to a new directory under `build/test-results/dev/`.
- A failed CTest run, a missing result or a duplicate result fails the runner.
  A skip stays a skip; it is not a pass.
- Summarize an older log without running tests:
  `python3 scripts/run_test_suite.py --report-log /absolute/path/to/ctest.log`.

Only one complete-suite runner can hold `build/.test-suite.lock` in a checkout
at a time. Direct `ctest` runs do not take that lock, so do not overlap them
with a complete run. The suite also configures and builds a separate
tests-disabled Release application each time, under
`build/dev/tests/end_to_end/composition-contract-build/release`; a cold
checkout pays for that first build.

> [!NOTE]
> A green result can still leave things unchecked. `coding_format_check` is
> *skipped*, not passed, without clang-format 22, and some Python rows pass
> while skipping individual cases. When you report a run, list skipped rows and
> the case lines found by
> `grep -h -e '^SKIPPED: ' -e '^EXPECTED FAILURE: ' build/dev/Testing/Temporary/LastTest.log`.
> Every `ctest` invocation rewrites `LastTest.log`, so search it before running
> `ctest` again. Details: [A passing row can leave cases unchecked](build-and-test-reference.md#a-passing-row-can-leave-cases-unchecked).

### The Qt-free core

```sh
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure
```

Or in one step: `cmake --workflow --preset core-only`. This graph excludes Qt
entirely and proves the core and application host build and pass without it.

### Other quick checks

```sh
cmake --build build/dev --target check_boundaries
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review --self-test
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --render-evidence build/dev/foundation-window.png
```

`check_boundaries` enforces the architecture's dependency and ownership rules.
The documentation gate checks required files and that every relative link
resolves; run it without a pipe, because `| tail` reports the pipe's exit
status, not the gate's. On Linux, substitute the flat application path
`build/dev/src/app/better_pull_review`.

## Coding checks

`_coding-standard/STANDARD.md` is the first-party C++ standard. The root
`.clang-format`, `.clang-tidy` and `.editorconfig` enforce it. The scripts scan
`src/` and `tests/`, excluding build output, generated sources, CMake
dependency trees and vendored code.

After configuring `dev`:

```sh
cmake --build --preset dev --target coding-format-check
cmake --build --preset dev --target coding-tidy
```

`coding-format` rewrites the whole first-party tree. To pass explicit tool
paths or a narrower set of paths, run the scripts directly:

```sh
python3 _coding-standard/scripts/format_check.py --root .
python3 _coding-standard/scripts/format.py --root .
python3 _coding-standard/scripts/tidy.py \
  --root . --build-dir build/dev --warnings-as-errors
```

Coding targets default to the value of `BUILD_TESTING`, so a tests-disabled
graph has no Python or coding-tool dependency; add
`-DBPR_ENABLE_CODING_TARGETS=ON` to include them anyway. The tidy profile's
exclusions are documented in `_coding-standard/README.md`.

### Format with clang-format 22

The tree is formatted by **clang-format 22, exactly**. Both format scripts and
the `coding_format_check` CTest row refuse any other major release: 18 to 21
render `{ }` as `{}`, 17 and earlier cannot read the style file, and 23 orders
includes and aligns assignments differently. A file formatted by 23 fails the
check on a machine running 22.

| Platform | Install | Where the gate finds it |
|---|---|---|
| macOS | `brew install llvm@22` | `/opt/homebrew/opt/llvm@22/bin/clang-format` (keg-only, so it does not displace Homebrew's default `llvm`) |
| Debian, Ubuntu | `sudo apt install clang-format-22` | `/usr/bin/clang-format-22` |
| Windows | LLVM 22.x, or a Visual Studio whose bundled LLVM is 22.x | `clang-format-22.exe` or `clang-format.exe` on `PATH` only |

Without `--clang-format`, the scripts take the first clang-format that reports
version 22 from `clang-format-22` on `PATH`, then
`/opt/homebrew/opt/llvm@22/bin`, `/usr/local/opt/llvm@22/bin` or
`/usr/lib/llvm-22/bin`, then `clang-format` on `PATH`. Configure runs the same
search and prints its choice:

```
-- Coding standard clang-format: /opt/homebrew/opt/llvm@22/bin/clang-format
```

To choose the tool yourself, pass `--clang-format` to the scripts or
`-DBPR_CLANG_FORMAT_EXECUTABLE=<path>` to configure.

Reformat with `coding-format` or `_coding-standard/scripts/format.py`, never a
bare `clang-format -i` from `PATH`. `--allow-unsupported-clang-format` runs
anyway but labels the result advisory; it is not a gate verdict.

### Warnings are errors

`cmake/Warnings.cmake` gives every first-party target `/W4 /permissive- /WX`
on MSVC and `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`
elsewhere, plus shadowing checks on Clang and GCC. Code that compiles cleanly
on macOS can still fail on Windows; fix the code, never narrow a flag. See
[Warning classes every compiler treats as errors](build-and-test-reference.md#warning-classes-every-compiler-treats-as-errors)
and the GCC 15 miscompilation in
[GCC 15 can destroy a braced aggregate member twice](build-and-test-reference.md#gcc-15-can-destroy-a-braced-aggregate-member-twice).

## Troubleshooting

**`error: unsupported clang-format: … reports version 23.1.1`** (or 21, or
another release). Install clang-format 22 as shown in
[Format with clang-format 22](#format-with-clang-format-22). A build tree
configured before the pin can have the wrong tool cached; the next configure
warns that `BPR_CLANG_FORMAT_EXECUTABLE` is not 22 and searches again.

**`no clang-format found; this project needs clang-format 22` on Windows.**
Visual Studio does not put its LLVM on `PATH`. Add it before configuring,
adjusting the edition and version segments to your installation, and check
that the bundled LLVM is 22.x:

```powershell
$env:PATH = "$env:PATH;${env:ProgramFiles}\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin"
```

**`coding_format_check` shows as skipped.** Configure found no clang-format 22,
so there is no verdict. Install it and reconfigure.

**A documentation script refuses to run.** It needs Python 3.10 or newer and
names the version and interpreter it found. Point `python3` at a newer
interpreter.

**Linux Secret Service rows fail with
`linux-secret-service-private-session:unavailable:<tool>-missing`.**
`linux_secret_service_native_setup`, `linux_secret_service_native` and
`linux_secret_service_native_cleanup` need `dbus-daemon`, `dbus-send` and
`gnome-keyring-daemon`; `linux_secret_service_refusal_control` needs
`dbus-daemon`. Install the packages listed under
[Prerequisites](#prerequisites). The fake-backed
`linux_secret_service_contract` needs none of them.

**Configuring without network access.** Reuse an existing build tree and
dependency cache in fail-closed offline mode:

```sh
cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON
```

A missing CPM script, archive or changed source is then fatal before any
download is attempted. For Qt offline, supply an official 6.12.0 prefix through
`BPR_QT_ROOT`. Details: [Dependency boundary](build-and-test-reference.md#dependency-boundary).

**A course prints `AGENT_WORKSPACE_RECOVERY_REFUSED:…:repair-required`.** The
course rooted its data directly in a shared temporary directory. Use the
owner-private data-root helpers described in
[A course's data root sits one owned level down](build-and-test-reference.md#a-courses-data-root-sits-one-owned-level-down).

**A row fails with `RUN_BUDGET_EXPIRED`, `PROCESS_TIMEOUT` or `***Timeout`.**
That is clock evidence, not necessarily a product regression. Record the host
load, then diagnose or repeat under declared conditions rather than retrying
until green. See [Run budgets and row timeouts](build-and-test-reference.md#run-budgets-and-row-timeouts).

**The complete-suite runner refuses to start.** Another runner in this checkout
holds `build/.test-suite.lock`. Wait for it to finish.

**Git admission fails at runtime.** BetterPullReview admits Git 2.39 or newer
only after its own capability probes pass. Repair or replace the Git
installation; do not add Git configuration, credential helpers or environment
variables to get past the probe. See
[Retained Git history and epoch storage](build-and-test-reference.md#retained-git-history-and-epoch-storage).

## Reference topics

The [build and test reference](build-and-test-reference.md) holds the detailed
material. Its main sections:

| Topic | Sections |
|---|---|
| Dependencies and providers | [Core dependency boundary](build-and-test-reference.md#core-dependency-boundary), [Dependency boundary](build-and-test-reference.md#dependency-boundary) (Qt, CPM, SQLite, KSyntaxHighlighting, offline mode) |
| Per-library notes | [Core SHA-256 selection](build-and-test-reference.md#core-sha-256-selection), [Basic local diagnostics](build-and-test-reference.md#basic-local-diagnostics) (Quill), [Whole-review JSON dependency](build-and-test-reference.md#whole-review-json-dependency), [JSON decoding](build-and-test-reference.md#json-decoding), [Core zlib](build-and-test-reference.md#core-zlib), [Core utf8proc](build-and-test-reference.md#core-utf8proc), [Core fmt and shared byte helpers](build-and-test-reference.md#core-fmt-and-shared-byte-helpers), [Core sha1collisiondetection](build-and-test-reference.md#core-sha1collisiondetection), [Core HACL\* SHA-2](build-and-test-reference.md#core-hacl-sha-2), [Core rapidfuzz-cpp line-pair similarity](build-and-test-reference.md#core-rapidfuzz-cpp-line-pair-similarity), [Linked xdiff primary line matcher](build-and-test-reference.md#linked-xdiff-primary-line-matcher), [Core dtl intraline ranges](build-and-test-reference.md#core-dtl-intraline-ranges), [Source search regex engine](build-and-test-reference.md#source-search-regex-engine), [Bundled code font](build-and-test-reference.md#bundled-code-font) |
| Test harness rules | [Run budgets and row timeouts](build-and-test-reference.md#run-budgets-and-row-timeouts), [Presets and the complete suite](build-and-test-reference.md#presets-and-the-complete-suite), [Automated-test network boundary](build-and-test-reference.md#automated-test-network-boundary), [Coding standard details](build-and-test-reference.md#coding-standard-details) |
| Attended and live courses | [Attended native notification course](build-and-test-reference.md#attended-native-notification-course), [Live publication course (RC59-10)](build-and-test-reference.md#live-publication-course-rc59-10), [The RFC-BPR-0016 device-flow guards](build-and-test-reference.md#the-rfc-bpr-0016-device-flow-guards), [RFC-BPR-0017 credential-free demonstration](build-and-test-reference.md#rfc-bpr-0017-credential-free-demonstration), [Public-preview operation and evidence](build-and-test-reference.md#public-preview-operation-and-evidence) |
| Assets and appearance | [Dif asset maintenance](build-and-test-reference.md#dif-asset-maintenance), [Native appearance verification](build-and-test-reference.md#native-appearance-verification) |
| Fuzzing and evaluation | [Parser fuzzing](build-and-test-reference.md#parser-fuzzing), [Hash evaluation](build-and-test-reference.md#hash-evaluation), [C++ parser evaluation](build-and-test-reference.md#c-parser-evaluation) |
| Per-RFC checks and history | [RFC-BPR-0023 private review checks](build-and-test-reference.md#rfc-bpr-0023-private-review-checks), [Current CLI milestone and historical checks](build-and-test-reference.md#current-cli-milestone-and-historical-checks), [Command lines](build-and-test-reference.md#command-lines), [RFC-BPR-0015 guarded CLI publication fixture](build-and-test-reference.md#rfc-bpr-0015-guarded-cli-publication-fixture), [Local Mac private alpha](build-and-test-reference.md#local-mac-private-alpha) |
| Runtime safety notes | [Troubleshooting and safe verification](build-and-test-reference.md#troubleshooting-and-safe-verification) (agent invocation, credentials, device-code sign-in, retained Git history) |

Related guides: [release packages](release-packages.md),
[remote stations](remote-stations.md), [Review Cycles CLI](cycle-cli.md) and
[optional secret scanning](betterleaks.md).
