---
schema: "implement-rfc.card.v1"
id: "BPR-0735"
title: "clangd cannot find the compile database and reports every header missing"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"owner_direction":"2026-09-11","mandate":"attended-prep-2026-09-10 r7"}
---

# clangd cannot find the compile database and reports every header missing

The owner, 2026-09-11: "Please add a card to fix the clangd noise". The IDE
diagnostics on `src/core/src/epoch_transition.cpp`, `src/app/main.cpp`,
`src/app/src/github_application.cpp`, `src/adapters/qt/src/workbench_*.cpp`
and the tests read `'bpr/...hpp' file not found`, `No member named
'span'/'ranges'/'string' in namespace 'std'`, `std::string_view (aka
'int')` and unknown `QString`/`QWidget`, while the real builds and strict
tidy pass.

## Cause

The project exports a compile database (`CMAKE_EXPORT_COMPILE_COMMANDS ON`
in `CMakeLists.txt:27` and the `dev` preset), but it lands in
`build/dev/compile_commands.json`. clangd looks for
`compile_commands.json` in the file's directory and its ancestors, and in
a `build/` child of each, never in `build/dev`; the repository has no
`.clangd` and no root `compile_commands.json` (that name is in
`.gitignore:9`). So every file falls back to clangd's default flags: no
include paths, no Qt, and no `-std=` at the project's level, which is why
the standard library names and even `std::string_view` come out wrong.
The diagnostics are all the same one symptom.

## Scope

- A committed `.clangd` at the repository root whose `CompileFlags:
  CompilationDatabase:` names `build/dev` (the ordinary configured tree
  every session has), so clangd reads the exact flags the build uses;
  add the `-std=` and platform flags there only if the database does not
  already carry them. `.clangd` is not build input and changes no
  target.
- Consider one `--target`/`Add`/`Remove` tweak if clangd chokes on an
  Apple-clang-only flag in the database; record it in the file.
- One paragraph in `docs/development/build-and-test.md` under the editor
  section (or a new short one): configure `dev` first, then open the
  tree; how to point at another preset's tree if needed.
- Not in scope: a root symlink to the database (it is ignored and per
  machine), tidy or the format gate (both already pass), and any change
  to the compiler flags in `cmake/Warnings.cmake`.
- Platform-agnostic; Windows uses the `windows-dev` preset's tree, which
  the paragraph names as the alternative.

## Acceptance

- With `.clangd` committed and `build/dev` configured, `clangd --check=`
  on `src/core/src/epoch_transition.cpp`, `src/app/main.cpp` and
  `tests/adapters_qt/epoch_comparison_control_tests.cpp` reports no
  `file not found`, no missing `std::` member and no unknown Qt type; the
  command and its output are recorded on this card.
- `python3 scripts/verify_documentation.py --root .` passes with the new
  paragraph; `git diff --check` is clean.
- The owner sees the noise gone in their editor on one of the named
  files (an observation on this card, no attended session needed).
