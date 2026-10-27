---
schema: "implement-rfc.card.v1"
id: "BPR-0735"
title: "clangd cannot find the compile database and reports every header missing"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"owner_direction":"2026-09-11","mandate":"attended-prep-2026-09-10 r7","triage_20261026":{"date":"2026-10-26","disposition":"done-with-declined-criterion","reason":"The .clangd fix shipped and passes clangd --check; the owner declined the editor observation."}}
acceptance: [{"criterion":"AC1 clangd named diagnostics absent","outcome":"pass","revision":"48296f42","evidence":{"kind":"command","detail":"Homebrew clangd 23.1 --check loaded build/dev/compile_commands.json for core epoch_transition.cpp, app/main.cpp and Qt epoch_comparison_control_tests.cpp. None reports missing bpr headers, missing std members or unknown Qt types. Logs build/logs/bpr-0735-d25-{core,app,qt}-final.log. Qt exit 0; core/app exit 3 only from clangd ExtractFunction/ExpandDeducedType tweak feature-test errors, not source diagnostics."}},{"criterion":"AC2 documentation and whitespace","outcome":"pass","revision":"48296f42","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK; git diff --check clean."}},{"criterion":"AC3 owner editor observation","outcome":"pending","revision":"8e5fe049","evidence":{"kind":"observation","detail":"Owner answered the named-file editor-observation ask under mandate r18 on 2026-09-12, verbatim: \"unsure, don't care\". This is not an observation that the noise is gone and cannot pass AC3. The owner declined this verification, so the same question is not repeated by default. The card is blocked on owner-declined-editor-observation until new direction changes the criterion or supplies relevant direct evidence."}}]
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

## Local candidate, 2026-09-12 (d25, mandate r16)

The root `.clangd` now selects `build/dev/compile_commands.json` and
suppresses only Homebrew clangd's `-Wmissing-designated-field-initializers`
diagnostic, which Apple clang accepts while the database's `-Werror` remains
in force. `docs/development/build-and-test.md` explains configuring `dev`
first and how to select another preset locally. The build flags and warning
policy are unchanged.

**Red before the fix:** `clangd --check=src/core/src/epoch_transition.cpp`
reported `bpr/core/epoch_transition.hpp` not found and cascaded unknown types
(`build/logs/bpr-0735-d25-red.log`). **After the fix:** full checks on that
file, `src/app/main.cpp`, and
`tests/adapters_qt/epoch_comparison_control_tests.cpp` each loaded
`build/dev/compile_commands.json` and reported no missing `bpr/` header,
standard-library member or Qt type (`build/logs/bpr-0735-d25-{core,app,qt}-final.log`).
The Qt test file's check exits zero. Homebrew clangd 23.1.0's full check mode
still exits 3 on five and eight `tweak: ExtractFunction/ExpandDeducedType`
feature-test errors in the core and app files; those logs contain no source
parse/type diagnostics. This is a limitation of that verification command,
not a claim that it exits clean. The documentation gate and `git diff --check`
pass.

The remaining acceptance is the owner's observation that the named editor
diagnostics have disappeared in one of those files. No attended packet is
needed. The implementation and local check evidence were published while
this criterion remained human-pending.

## Owner reply and disposition, 2026-09-12 (mandate r18)

Asked for an editor observation in one named file, the owner answered,
verbatim: **"unsure, don't care"**. That does not say the diagnostics are
gone, so AC3 remains pending. The card moves to Blocked on
`owner-declined-editor-observation`: there is no active human-review session
or repeat of this same ask by default. Completion needs new owner direction
that changes this acceptance criterion, or a relevant direct observation;
neither is inferred from the reply.

## Closed in triage, 2026-10-26

Closed. The root `.clangd` (`48296f429`) and the build guide's paragraph
are on main, and the `clangd --check` results for all three named files
are recorded above. The owner answered the remaining editor-observation
criterion with "unsure, don't care" on 2026-09-12. That is recorded as
declined, not observed. Six weeks on, nothing has reported the noise
again, so the card is closed on that answer instead of staying Blocked
indefinitely.
