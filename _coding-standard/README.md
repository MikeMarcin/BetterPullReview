# BetterPullReview Coding Standard

This directory contains BetterPullReview's code-level coding-standard tooling. It
uses local tools only: clang-format, clang-tidy, EditorConfig, CMake
`compile_commands.json`, and Python.

The canonical scripts live under `_coding-standard/scripts/`. The canonical
`.clang-format`, `.clang-tidy`, and `.editorconfig` files live at the repository
root.

Installing these files does not migrate an existing codebase. Follow
`_coding-standard/ADOPTION.md` to rename first-party C++ identifiers, document
the public surface, preserve external formats, and verify the project before
calling the standard adopted.

## Direct Script Use

From a project root:

```sh
python3 _coding-standard/scripts/format_check.py --root .
python3 _coding-standard/scripts/format.py --root .
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors
```

Pass explicit paths after the options to limit the scan.

## Supported clang-format Versions

`.clang-format` is rendered by **clang-format 22 or newer**, recorded as
`MINIMUM_CLANG_FORMAT_VERSION` in `scripts/_coding_standard.py`. Both
`format_check.py` and `format.py` read the tool's reported version first and
refuse an older one, naming what they found and what they need.

The boundary is not arbitrary, and it has two independent causes:

- **17 and earlier cannot read the style file.** `AlignFunctionPointers` was
  added in 18, so an older tool reports `unknown key` and silently falls back to
  a default style. Every file then looks dirty for a reason unrelated to the
  file.
- **18 through 21 read it and render it differently.** They emit `{}` where this
  standard's sources hold `{ }`, and no key restores the space —
  `Cpp11BracedListStyle` produces `{}` either way in those releases, and
  `SpaceInEmptyBraces` is not an option they accept. Every file containing `{ }`
  therefore disagrees, which is most of a migrated tree.

The second tier is the dangerous one: it produces a plausible-looking result.

Pass `--allow-unsupported-clang-format` to run anyway. The diagnostic is still
printed and the result is labelled advisory: a tool that renders the style file
differently can report a file's state, but not the project's.

## CMake Integration

Top-level project builds include:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/_coding-standard/cmake/CodingStandard.cmake")
```

This adds non-default local targets:

- `coding-format`
- `coding-format-check`
- `coding-tidy`

BetterPullReview adapts the neutral helper to reuse the centrally discovered
`Python3::Interpreter` target. `BPR_ENABLE_CODING_TARGETS` defaults to the
value of `BUILD_TESTING`; an explicitly enabled tests-disabled graph therefore
discovers Python through the same audited dependency provider, while a normal
tests-disabled product graph adds no tooling dependency.

First-party executable CMake targets use the standard `app_` and `test_`
categories. The root `bpr_add_executable` helper removes that category for
`OUTPUT_NAME`, preserving the established application and test binary names.

The clang-tidy helper also neutralizes the compilation database's product
`-Werror` flag. This keeps `--warnings-as-errors` scoped to clang-tidy findings
while preserving hard compiler failures; it additionally suppresses Clang's
known partial-designated-initializer warning so aggregate fixtures can be
analyzed under the same language mode as the build.

The repository clang-tidy profile also records narrow compatibility
adaptations for the existing product contracts. It excludes checks that would
change public enum representation, replace deliberate explicit state
initialization, reject value-semantic `std::stop_token` parameters and explicit
ownership-transfer moves, misdiagnose guarded optional access or intentional
fail-closed exception containment, or force standard algorithms where a clear
imperative loop is the accepted local expression. Correctness-oriented
analyzer, bugprone, portability, performance, modernization, readability, and
identifier checks remain enabled and run with warnings promoted to errors.

Every public first-party class, value type, request/event/projection record,
and namespace-level function has a Doxygen brief. Type-level contracts cover
trivial accessors, comparisons, and record fields whose semantics are fully
expressed by their names and types; non-trivial ownership, authority,
lifecycle, and failure behavior retains its detailed adjacent contract.

## External Dependencies

The optional `_coding-standard/cmake/CodingStandardCpm.cmake` bootstrap pins CPM
and keeps downloaded source trees in a repository-local cache. Network source
acquisition goes through `CPMAddPackage`.

`CodingStandardFmt.cmake` and `CodingStandardGTest.cmake` first reuse installed
packages and can fetch immutable pinned revisions when their project-owned fetch
options are enabled.

## Source Discovery

With no path arguments, the scripts scan project roots that exist among:

- `include`
- `src`
- `modules`
- `examples`
- `benchmarks`
- `tests`

Build outputs, generated sources, vendored dependencies, CMake `_deps`, and VCS
metadata are skipped.
