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

## The clang-format Version: 22, Exactly

The tree is rendered by **clang-format 22**, and only 22. The required major
release is `REQUIRED_CLANG_FORMAT_MAJOR` in `scripts/_coding_standard.py`. Both
`format_check.py` and `format.py` read the tool's reported version first and
refuse any other major release. The refusal names the version found, the tool's
path, and how to install 22.

Without `--clang-format`, the scripts look for a 22 themselves, in this order:

1. `clang-format-22` on `PATH` (Debian and Ubuntu's `clang-format-22` package);
2. `clang-format` in `/opt/homebrew/opt/llvm@22/bin`,
   `/usr/local/opt/llvm@22/bin` and `/usr/lib/llvm-22/bin` (Homebrew's
   keg-only `llvm@22`, and Debian's LLVM 22 directory);
3. `clang-format` on `PATH`.

They take the first one that reports 22. When none does, they run
`clang-format` from `PATH` so that the refusal names the tool you would
otherwise have run.

To install 22:

| Platform | Command | Tool |
|---|---|---|
| macOS | `brew install llvm@22` | `/opt/homebrew/opt/llvm@22/bin/clang-format` |
| Debian, Ubuntu | `sudo apt install clang-format-22` | `/usr/bin/clang-format-22` |
| Windows | Install LLVM 22.x, or use a Visual Studio whose bundled LLVM is 22.x | Pass `--clang-format`, or `-DBPR_CLANG_FORMAT_EXECUTABLE` to configure |

A range of releases is not enough, because each major release renders the
same style file differently:

- **17 and earlier cannot read the style file.** `AlignFunctionPointers` was
  added in 18, so an older tool reports `unknown key` and silently falls back to
  a default style. Every file then looks dirty for a reason unrelated to the
  file.
- **18 through 21 read it and render it differently.** They emit `{}` where this
  standard's sources hold `{ }`, and no key restores the space —
  `Cpp11BracedListStyle` produces `{}` either way in those releases, and
  `SpaceInEmptyBraces` is not an option they accept. Every file containing `{ }`
  therefore disagrees, which is most of a migrated tree.
- **23 reads it and renders some files differently.** It orders some include
  blocks differently, aligns runs of consecutive assignments differently, and
  indents a designated initializer's continuation lines differently. When the
  gate accepted "22 or newer", the Mac's Homebrew default moved to 23.1.1 while
  Linux stayed on 22.1.8. Four files formatted on the Mac then failed Linux's
  `coding_format_check` ([[BPR-1352]]).

The second and third tiers are the dangerous ones: they produce a
plausible-looking result.

Pass `--allow-unsupported-clang-format` to run anyway. The diagnostic is still
printed and the result is labelled advisory: a tool that renders the style file
differently can report a file's state, but not the project's.

When LLVM 22 is retired, move the pin deliberately. Change
`REQUIRED_CLANG_FORMAT_MAJOR`, the CMake lookup's names and hints, and the
banners in `tests/scripts/coding_standard_format_gate_tests.py`. Then reformat
the tree with the new release in one format-only commit, and have every station
install that release.

## CMake Integration

Top-level project builds include:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/_coding-standard/cmake/CodingStandard.cmake")
```

This adds non-default local targets:

- `coding-format`
- `coding-format-check`
- `coding-tidy`

Configure finds the clang-format these targets run
(`cmake/CodingStandardClangFormat.cmake`). It searches the same places, in the
same order, as the scripts above: `clang-format-22`, then the pinned install
directories, then `clang-format` on `PATH`. It asks each candidate whether it is
22, using `scripts/supported_clang_format.py`, and passes over any other
release. So a Mac whose `PATH` gives Homebrew's 23 still gets the `llvm@22`,
and a station whose `PATH` puts 21 ahead of a 22 still gets the 22. The result
is cached as `BPR_CLANG_FORMAT_EXECUTABLE`, and you can set that variable to
choose a tool. A cached tool that is not 22, such as a tree configured before
the pin, is reported with a warning and searched for again. When configure
finds none, the targets fall back to the scripts' own search, and the gate
refuses as described above.

The same tool runs the repo-wide check as the CTest row `coding_format_check`,
through `scripts/format_check_or_skip.py`. The row fails when a file is
unformatted. It reports **Skipped**, not failed, when configure found no
clang-format 22 or the tool it found is no longer 22. That result is not
a formatting verdict.

BetterPullReview adapts the neutral helper to reuse the centrally discovered
`Python3::Interpreter` target. `BPR_ENABLE_CODING_TARGETS` defaults to the
value of `BUILD_TESTING`; an explicitly enabled tests-disabled graph therefore
discovers Python through the same audited dependency provider, while a normal
tests-disabled product graph adds no tooling dependency.

First-party executable CMake targets use the standard `app_` and `test_`
categories. The root `bpr_add_executable` helper removes that category for
`OUTPUT_NAME`, preserving the established application and test binary names.

The clang-tidy helper discovers LLVM 22 by checking `clang-tidy-22`, the pinned
LLVM directories used by the formatter, and then `clang-tidy` on `PATH`. It
refuses a different default major release. `--clang-tidy` selects an explicit
tool for separate analysis. Set `CODING_STANDARD_TIDY_JOBS` for bounded parallel
analysis through the CMake target, or use `tidy.py --jobs`; fixes require one
process because translation units can share headers.

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
