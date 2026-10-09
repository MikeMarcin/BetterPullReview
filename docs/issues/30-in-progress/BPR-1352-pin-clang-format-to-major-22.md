---
schema: "implement-rfc.card.v1"
id: "BPR-1352"
title: "Pin clang-format to major release 22, so a file formatted on one station passes every station's format gate"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"coordinator, 2026-10-08: main red on Linux's coding_format_check for four files formatted on the Mac with Homebrew's clang-format 23.1.1","reservation":"docs/issues/_reservations/BPR-1352--BPR-1352--15c5b707b1ee.json","related_cards":["BPR-0367","BPR-0865","BPR-1243"]}
---

# BPR-1352: Pin clang-format to major release 22, so a file formatted on one station passes every station's format gate

## Observation

The repository pinned only a minimum clang-format: "22 or newer"
(`MINIMUM_CLANG_FORMAT_VERSION` in `_coding-standard/scripts/_coding_standard.py`,
`_coding-standard/README.md`, `docs/development/build-and-test.md`). The Mac's
default clang-format is now Homebrew's `llvm`, 23.1.1, and the Linux station
runs 22.1.8. The two releases render the same `.clang-format` differently, so a
file formatted on one fails the other's `coding_format_check`.

On 2026-10-08 main was red on Linux for four files:

| File | What 22 changes back |
|---|---|
| `src/adapters/qt/src/anchored_tooltip.cpp` | Include order (`<QVariant>`) |
| `tests/adapters_qt/workbench_window_tests.cpp` | Consecutive-assignment alignment |
| `tests/core/workbench_interaction_tests.cpp` | Designated-initializer continuation indent, and consecutive-assignment alignment |
| `tests/end_to_end/change_arrows_across_files_public_path_tests.cpp` | Include order (`<QStatusBar>`), and consecutive-assignment alignment |

[[BPR-1243]] had already met the same disagreement in
`github_application.cpp`, and worked around it by hand.

## Outcome

- **The gate takes 22 and no other major release.**
  `REQUIRED_CLANG_FORMAT_MAJOR = 22` replaces the minimum. `format_check.py`
  and `format.py` refuse 21 and earlier as before, and now refuse 23 and later
  too. The refusal names:
  - the version found and the tool's path;
  - why another release cannot give a verdict;
  - how to get 22 (`brew install llvm@22`, `apt install clang-format-22`, LLVM
    22.x on Windows).
- **The scripts find a 22 when none is named.** They take the first of these
  that reports 22:
  1. `clang-format-22` on `PATH`;
  2. `clang-format` in `/opt/homebrew/opt/llvm@22/bin`,
     `/usr/local/opt/llvm@22/bin` or `/usr/lib/llvm-22/bin`;
  3. `clang-format` on `PATH`.

  Without a 22, they run `PATH`'s clang-format, so the refusal names it.
- **Configure searches the same places, in the same order**
  (`CodingStandardClangFormat.cmake`: `NAMES clang-format-22 clang-format`, the
  pinned directories as `HINTS`, the version check as `VALIDATOR`). A test
  holds the two lists together. A cached `BPR_CLANG_FORMAT_EXECUTABLE` that is
  not 22 is reported with a CMake warning and searched for again. That case is
  every Mac build tree configured before the pin, which cached Homebrew's 23.
- `coding_format_check` reports Skipped, never a verdict, when configure's tool
  is not 22.
- The four files are reformatted with 22.1.8 in a separate format-only
  commit, and the whole tree passes the gate under 22.
- Documentation updated in `_coding-standard/README.md`,
  `docs/development/build-and-test.md`, `README.md`, the root `AGENTS.md`
  (Commands) and the `coding_format_check` comment in `tests/CMakeLists.txt`.

## Acceptance

- `coding_standard_format_gate_contract` refuses 21 and 23 in both scripts.
  It skips the row for 23, accepts 22, and pins both searches, including the
  Mac's case: Homebrew's 23 first on `PATH`, with `llvm@22` holding the 22.
  The new cases fail against the unpinned scripts.
- With no stubs, configure and the scripts on the Mac pick
  `/opt/homebrew/opt/llvm@22/bin/clang-format` (22.1.8), although `PATH` gives
  Homebrew's 23.1.1 first.
- The whole tree passes `format_check.py` under clang-format 22.1.8 after the
  format-only commit.
- The documentation gate passes.

## Owed to bpr-main-win

The Windows station must confirm its clang-format version. The pinned
directories are Homebrew and Debian paths. On Windows the search finds only
`clang-format-22.exe` or `clang-format.exe` on `PATH`, or the tool named by
`-DBPR_CLANG_FORMAT_EXECUTABLE`, and that tool must now report **22**. If
Visual Studio's bundled LLVM (`VC\Tools\Llvm\x64\bin`) has moved to 23, the
row reports Skipped, and the scripts refuse. In that case install LLVM 22.x
and name it.

To confirm, reconfigure `windows-dev` and check:

- the `Coding standard clang-format:` line names a 22;
- `ctest --preset windows-dev -R coding_format -V` passes, with
  `coding_format_check` passed rather than skipped.

The discovery tests in `configure_finds_clang_format_22` are skipped on
Windows, as before: `find_program` does not run the batch-file stubs. The
Python discovery cases run there.
