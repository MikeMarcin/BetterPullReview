# Build and Test

## Prerequisites

- CMake 3.25 or newer
- Ninja
- A C++23 compiler
- Qt 6.8 or newer for the Qt adapter and application
- Python 3 for structural and runtime verification scripts

The validated local foundation environment uses CMake 4.3, AppleClang 16,
Ninja, Python 3.14, and Qt 6.11.1 on macOS arm64.

## Presets

`dev` configures the complete foundation, including Qt and all tests:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

`core-only` excludes the Qt target graph and proves that the core and
application host build without Qt:

```sh
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure
```

Both presets use out-of-source directories under ignored `build/`.

Windows CI uses the conditional multi-configuration preset that selects the
Visual Studio 2022 x64 generator and the matching Qt archive:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure
```

## Targets

| Target | Responsibility |
|---|---|
| `better_pull_review_core` | Qt/effect-free semantic library |
| `better_pull_review_application` | Host, ports, serialized completion, manual executor |
| `better_pull_review_adapter_{github,git,sqlite,ai}` | Capability boundaries established for later RFCs |
| `better_pull_review_adapter_qt` | Native Qt presentation adapter |
| `better_pull_review_composition` | Concrete foundation wiring |
| `better_pull_review` | Native application executable |

## Focused evidence

```sh
cmake --build build/dev --target check_boundaries
python3 scripts/verify_documentation.py --root .
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review
build/dev/src/app/better_pull_review --self-test
build/dev/src/app/better_pull_review \
  --render-evidence build/dev/foundation-window.png
```

`verify_runtime_surface.py` runs on macOS, Linux, and Windows. The
binary-dependency script also supports all three platforms, using `dumpbin` on
Windows when available, followed by `llvm-readobj` and `objdump` fallbacks.

## CI

`.github/workflows/ci.yml` configures Qt 6.11.1 builds and application launch
tests on Ubuntu 24.04, Windows 2025, and macOS 15, plus a separate Qt-free core
job. Windows selects the Visual Studio 2022 x64 preset and
`win64_msvc2022_64`. Every directly selected action is pinned to a full commit
hash, and checkout credentials are not persisted.

A configured job is not passing evidence until its actual workflow run
completes successfully.
