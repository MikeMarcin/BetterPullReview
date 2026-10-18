# BetterPullReview Coding Standard

This package defines the coding standard for BetterPullReview's C++ code. The
tools provide conservative local enforcement: they catch formatting and
high-signal diagnostics without silently reformatting or renaming whole
libraries.

The canonical `.clang-format`, `.clang-tidy`, and `.editorconfig` files live at
the repository root.

## Source Roots

With no explicit paths, the scripts scan:

- `include/`
- `src/`
- `modules/`
- `examples/`
- `benchmarks/`
- `tests/`

The scripts skip build outputs, vendored dependencies, generated directories,
CMake `_deps`, and hidden VCS directories.

The tidy gate discovers clang-tidy 22 from its versioned command, the pinned
LLVM directories used by the format gate, or `PATH`, checking its version.
An explicit `--clang-tidy` can select another tool for a separate analysis.
The tidy gate can run read-only analysis concurrently: set
`CODING_STANDARD_TIDY_JOBS` for `coding-tidy`, or pass `--jobs` to `tidy.py`.
The default is one process. Diagnostics stay grouped by translation unit and
every failed unit fails the gate. `--fix` requires one process because units
can share headers.

## BetterPullReview Style

- The repository `.clang-format` is the formatting source of truth. It
  uses `IndentWidth: 4`, `TabWidth: 4`, and
  `UseTab: ForContinuationAndIndentation`.
- Braces use an Allman style for classes, structs, and functions.
- Control-flow opening braces stay on the same line as `if`, `for`, `while`,
  and `switch`.
- Namespaces use compact openings, for example `namespace bpr {`, with
  namespace contents indented.
- Access specifiers are outdented one level from class members.
- Pointers and references bind left, for example `Thing*` and `const Thing&`.
- Constructor initializer lists use deterministic continuation indentation.
- Includes are grouped intentionally; clang-format preserves include blocks and
  sorts inside each block.
- There is no hard line-length limit. `ColumnLimit: 0` avoids broad churn in
  existing code and examples.
- Naming conventions:
  - class, struct, function, and namespace names use `snake_case`;
  - function parameters use `aPrefix`;
  - local variables use `lowerCamelCase`;
  - member variables use `UpperCamelCase`;
  - template type parameters commonly use a leading `T`;
  - macros use uppercase names.
- String formatting uses `fmt::format` for one-shot strings and
  `fmt::format_to` for buffer appends. Do not introduce `std::ostringstream`,
  `std::stringstream`, or stream insertion as a string-formatting mechanism for
  returned strings, diagnostics, protocol payloads, or JSON text. Standard
  streams remain valid for file I/O, binary adapters, and token parsing.
- Non-trivial public classes and functions use Doxygen comments with
  `/// \brief`; use `/// \details`, `\param`, `\returns`, and `\tparam` when
  they materially clarify the contract.
- Tests should keep their existing framework naming shape while new helper
  functions and local types follow the BetterPullReview naming rules.

## Automatically Enforced

`.clang-format` enforces the mechanically expressible formatting rules:

- the established indentation policy (`IndentWidth: 4`, `TabWidth: 4`,
  `UseTab: ForContinuationAndIndentation`);
- Allman braces for classes, structs, and functions;
- same-line braces for control-flow statements;
- compact nested namespace openings with indented contents;
- left pointer/reference alignment;
- constructor initializer continuation style;
- preserved include blocks with deterministic include sorting inside blocks;
- line-ending and end-of-file behavior from the canonical formatter.

`.editorconfig` enforces editor-independent basics:

- UTF-8;
- LF line endings;
- final newline;
- trailing whitespace trimming, except Markdown where hard line breaks may be
  intentional;
- conservative indentation settings for C++, CMake, Python, JSON/YAML, and
  Markdown.

`.clang-tidy` starts with high-signal checks:

- `clang-analyzer-*`;
- `bugprone-*`, with noisy checks disabled;
- `performance-*`;
- `portability-*`;
- selected `modernize-*` checks that are usually mechanical and low-risk;
- selected `readability-*` checks for obvious correctness, maintainability, and
  the project naming conventions.

The tidy configuration avoids formatting-owned style checks. Identifier naming
is enforced, so installing this package into an existing project requires a
source migration; follow `_coding-standard/ADOPTION.md` before treating the
installation as complete.

## Agent-Applied Conventions

These rules are part of the standard even when the local tools cannot enforce
them reliably. Human reviewers and coding agents should apply them to new and
touched code.

### Naming

- Use `snake_case` for class, struct, function, namespace, file-local helper,
  and free-function names.
- Use a leading `T` for template type parameters when the surrounding code does
  so, for example `TValue`, `TAlloc`, or `TInputIt`.
- Use `aPrefix` for function parameters. The prefix is lowercase `a` followed
  by an uppercase semantic name, for example `aBundle`, `aRegistry`,
  `aFrame`, or `aPolicy`.
- Use `lowerCamelCase` for local variables, loop variables, and temporary
  values.
- Use `UpperCamelCase` for member variables, including private members. Do not
  introduce `m_`, trailing underscores, or other member markers unless a local
  type already uses them and the edit is intentionally preserving that local
  pattern.
- Use uppercase macro names with project prefixes.
- Prefer the nearest modern local constant naming convention. Do not rename
  existing constants as incidental cleanup.

### CMake Targets

- First-party executable targets use short category prefixes so build and launch
  selectors stay scannable: `test_`, `bench_`, `demo_`, `app_`, `tool_`, and
  `fuzz_`.
- Libraries, aliases, install/export names, package targets, third-party
  targets, and coding-standard custom targets keep their established naming
  shape.
- When a target rename would churn a documented command or user-facing binary,
  keep the executable filename stable with `OUTPUT_NAME`.

### Doxygen And Comments

- Add `/// \brief` to non-trivial public classes, public structs, public member
  functions, and public free functions.
- Add `/// \details` when a one-line brief does not explain ownership,
  threading, lifetime, invariants, preconditions, or failure behavior.
- Use `\param`, `\returns`, and `\tparam` when argument meaning, return
  semantics, or template constraints are not obvious from the signature.
- Prefer short comments that explain intent, invariants, or tradeoffs. Do not
  narrate simple statements.
- Keep comments current with the code. If a behavior changes, update or remove
  the old comment in the same edit.

### No Raw Loops

- A loop is acceptable inline when it is the main point of a short function.
- If a function contains a loop plus more than about three additional
  statements, conditionals, state updates, or error paths, strongly consider
  extracting the loop into a named helper.
- The helper name should describe the contract, not the mechanics, for example
  `collect_pending_commands`, `find_matching_agent`, or
  `build_evidence_summary`.
- Prefer range-for for imperative iteration when indexes are not semantically
  required.
- Prefer standard algorithms when they make intent clearer. Do not replace a
  clear loop with a dense algorithm expression just to satisfy this guideline.

### String Formatting

- Prefer `fmt::format` when constructing a complete string and
  `fmt::format_to` when appending into an existing string or buffer.
- Do not use `std::ostringstream`, `std::stringstream`, or stream insertion for
  returned strings, diagnostics, protocol payloads, or JSON text.
- Keep standard streams for actual stream work: file output, binary stream
  adapters, and token parsing such as `std::istringstream`.
- Avoid adding `fmt` to installed library targets solely for deterministic JSON
  serialization when direct string appenders preserve the public package
  surface.

### Include Grouping

- Preserve meaningful include blocks. Do not collapse all includes into one
  block merely because clang-format can sort them.
- In `.cpp` files, include the matching header first when one exists.
- Keep standard/library includes, third-party includes, project includes, and
  relative local includes in separate blocks when that structure is present.
- Add new includes to the narrowest existing block that matches their role.
- Remove unused includes when touching a file, but avoid broad include cleanup
  outside the current task.

### API And Policy Restraint

- Do not add public APIs unless the task requires them or the existing design
  has no narrower extension point.
- Keep unavoidable public additions small, named explicitly, and documented with
  the ownership and compatibility contract.
- Do not add test-only hooks, compile-time switches, or backdoors unless the
  user explicitly asks for them.
- Do not change matching, ranking, scheduling, validation, or policy behavior as
  a side effect of formatting or cleanup work.
- Keep refactors local to the requested module or ownership boundary. Avoid
  broad state-machine or architecture rewrites unless the task explicitly calls
  for that scope.
- When existing code disagrees with this standard, migrate a coherent dependency
  boundary together. Do not leave a declaration and its consumers using mixed
  conventions, and do not rewrite serialized keys, protocol text, or third-party
  identifiers as if they were first-party C++ names.

## Applicability

After adoption, all first-party BetterPullReview C++ under the source roots is
expected to follow these rules. Historical source filenames may retain their
existing names until a dedicated file-layout migration is planned.
