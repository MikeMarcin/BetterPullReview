---
schema: "implement-rfc.card.v1"
id: "BPR-0336"
title: "Repair the never-compiled Windows branches and MSVC portability defects"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: []
---

# Repair the never-compiled Windows branches and MSVC portability defects

A real Windows 10/11 x64 machine became available on 2026-08-17, so the
`platform-windows` blocker that [[BPR-0036]] records could be tested for the
first time. This card is what the first compilation found, and it is not
setup friction — every item below is a defect that shipped because no toolchain
this project had access to could see it.

Immutable base for the platform-enablement run: `f0003808d63e32699a3e3db0f774ce1fc872df9e`.

## Host

| Component | Version |
|---|---|
| OS | Windows 11 Home 10.0.26200, x64 |
| Compiler | MSVC 19.51.36248 (toolset 14.51.36231), Visual Studio Community 2026 (18.8.0) |
| Windows SDK | 10.0.26100.0 |
| CMake | 4.4.2 (standalone; the VS-bundled 4.3.1-msvc1 is below the project floor) |
| Ninja | 1.13.2 |

## What was wrong

### 1. The `windows-dev` preset named a generator that is not installed

`CMakePresets.json` pinned `Visual Studio 17 2022`. Only Visual Studio 2026 is
present, so the one preset [[BPR-0036]] names could not configure at all.
Retargeted to `Visual Studio 18 2026`, which CMake 4.4.2 supports and selects by
default. This narrows the preset to VS 2026; that is deliberate, per owner
direction.

### 2. `_WIN32` branches referred to identifiers that no longer exist

This is the same defect class [[BPR-0155]] found in `__linux__` branches, on the
other platform, and it went unnoticed for the same reason: the branches compile
nowhere this project could build. The BPR-0085 coding-standard pass renamed
locals to camelCase and the `_WIN32` blocks kept the old snake_case spellings.

In `tests/sqlite_contract/epoch_catalog_storage_tests.cpp`, three undeclared
names — `cancelled_root` (9 uses), `state_link`, `database_link` — produced 12
`C2065` errors and cascaded into 5 `C3536`, 4 `C2737`, 1 `C2664`, and 2 `C2440`.
The cascade was verified rather than assumed: after correcting only the three
identifiers, the translation unit compiles with zero diagnostics. Compiling the
whole unit also proved the later `_WIN32` blocks at lines 818+ and 900+ are
clean; MSVC had aborted before ever reaching them.

### 3. Five `C4456` shadowing declarations, fatal under `/W4 /WX`

MSVC diagnoses local shadowing that the project's other compilers do not, and
the warning set is fatal. Inner locals renamed, uses updated, behaviour
unchanged:

| File | Line | Rename |
|---|---|---|
| `tests/sqlite_contract/epoch_catalog_storage_tests.cpp` | 300 | `query` -> `scanQuery` |
| `tests/core/epoch_policy_tests.cpp` | 2112 | `before` -> `sessionBefore` |
| `tests/core/epoch_policy_tests.cpp` | 2200 | `state` -> `ineligibleState` |
| `src/adapters/sqlite/src/epoch_catalog.cpp` | 746 | `cleanup` -> `cleanupStatement` |
| `src/adapters/sqlite/src/epoch_catalog.cpp` | 3643 | `cache` -> `rowCache` |

### 4. An MSVC name-mangling divergence that cannot fail on Clang or GCC

Fixing the above let the graph reach a link edge for the first time, which then
failed:

```
review_ledger_storage_tests.cpp.obj : error LNK2019: unresolved external symbol
 set_review_ledger_preflight_boundary_hook_for_testing(void (__cdecl*)(const std::filesystem::path&))
better_pull_review_sqlite_review_ledger_tests.exe : fatal error LNK1120: 2 unresolved externals
```

The definitions were present in the library, under a decorated name differing by
one character: `Q6A...` where the reference wanted `P6A...`. The definitions in
`src/adapters/sqlite/src/review_ledger_database.cpp` spelled the parameter
`const preflight_test_hook aHook`; the only declaration that exists — a local
forward declaration in the test at `review_ledger_storage_tests.cpp:37-40` —
spells it without `const`. There is no header declaring these symbols, so the
two spellings never met until MSVC linked them.

Per `[dcl.fct]/5` a top-level cv-qualifier on a parameter is not part of the
function type, so these are the same function. MSVC encodes the top-level
`const` into the decorated name anyway; the Itanium ABI used by Clang and GCC
does not. **This class of defect is undetectable on macOS and Linux by
construction.** Resolved by removing the top-level `const` from the two
definitions, which is the standard-conformant spelling and matches the only
declaration any consumer sees.

## Evidence

- `cmake --preset core-only` configures.
- `cmake --build --preset core-only -- -k 0` completes with **zero failed
  edges** and zero warnings; an immediately repeated build reports
  `ninja: no work to do`.
- `clang-format --output-replacements-xml` reports zero replacements for all
  four changed source files.
- `dumpbin /symbols` and `dumpbin /linkermember:1` were used to establish the
  `Q`-versus-`P` decoration difference directly rather than inferring it.

## What this card does not claim

- **No Windows runtime property is proven.** `ctest --preset core-only` runs 84
  tests and all 84 fail identically with `Automated test launch refused: the
  Windows default-deny network boundary is unavailable`. Zero test binaries
  executed. That gate is [[BPR-0235]]'s Windows half and is tracked separately;
  until it lifts, no Windows behavioural claim can be made from this host.
- **Only the Qt-free graph is proven.** `BPR_BUILD_QT=ON` targets are untouched
  by this card. The `dev` graph is far larger and has never been compiled on
  Windows, so more defects of classes 2-4 should be expected there.
- This card does not close [[BPR-0036]], which owns the full target-native
  verification matrix, nor does it move any RFC status.

## Follow-up

Any cross-translation-unit pair where a definition spells a pointer parameter
`const` and its declaration does not will fail identically on MSVC. No other
instance manifests in `core-only`; the Qt graph is unverified. Rather than grep
for the pattern, the Windows link step is the reliable detector, so this is left
to surface during the `dev` build instead of being pre-emptively swept.
