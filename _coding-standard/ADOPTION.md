# Adopting The BetterPullReview Coding Standard

Installing the configuration files is only the start of adoption. An existing
project must migrate its first-party source code to the conventions in
`STANDARD.md`; do not describe the standard as adopted while public headers or
their consumers still use the old naming and documentation style.

The installer renders project identifiers in the tooling and examples. It does
not rename the destination project's C++ symbols, CMake targets, files, tests,
serialized fields, or protocol text.

## 1. Establish The Migration Boundary

Inventory first-party code under `include`, `src`, `modules`, `examples`,
`benchmarks`, and `tests`. Exclude generated and vendored code.

Migrate one coherent dependency boundary at a time:

1. a public header and every first-party source, test, and example that consumes
   it;
2. the corresponding implementation's file-local types, helpers, parameters,
   locals, and members;
3. the public Doxygen contracts for that boundary;
4. a complete build and test run before moving to the next boundary.

For a small project, the whole first-party source tree may be the safest single
boundary. A mixed state in which declarations and consumers follow different
conventions is not an adopted state.

## 2. Rename C++ Identifiers Semantically

Apply the naming rules from `STANDARD.md` throughout the boundary:

- class, struct, enum, type-alias, function, method, helper, and namespace names
  use `snake_case`;
- function parameters use `aPrefix`;
- local variables use `lowerCamelCase`;
- non-constant member variables use `UpperCamelCase`;
- template type parameters commonly use a leading `T`;
- first-party macros use uppercase names with the project's macro prefix.

Use compiler-, IDE-, or AST-aware rename tooling when possible. Review every
replacement. A blind textual replacement can corrupt unrelated names, including:

- serialized JSON, TOML, YAML, command-line, or protocol keys;
- user-facing output and stable operation identifiers;
- third-party types, methods, enum values, and callback signatures;
- test-framework-generated names;
- a type and a local variable that happen to share a word.

The installed `.clang-tidy` enables `readability-identifier-naming`. Use it as
an audit and fix findings in dependency order; do not accept automated fixes
without compiling and reviewing the diff. Pay particular attention to parameter
names whose semantic word already starts with `a`: `attributes` should become
`aAttributes`, not a mechanically truncated spelling such as `aTtributes`.

## 3. Document The Public Surface

For each non-trivial public class, struct, member function, and free function:

- add a concise `/// \brief` contract;
- add `/// \details` for ownership, lifetime, threading, invariants,
  preconditions, or failure behavior when needed;
- add `\param`, `\returns`, `\throws`, and `\tparam` only where they clarify
  behavior that the signature does not make obvious.

Documentation must describe the migrated API, not the names or behavior it had
before adoption.

## 4. Apply The Remaining Conventions

Run clang-format, then review include grouping, raw loops, string construction,
CMake executable target names, and API additions against `STANDARD.md`. Formatting
alone does not establish compliance with these review-applied rules.

Keep stable external behavior unchanged unless the project explicitly chooses a
breaking change. In particular, verify serialized output and import/export
round-trips after identifier migration.

## 5. Verify Adoption

With a compilation database available, run:

```sh
python3 _coding-standard/scripts/format.py --root .
python3 _coding-standard/scripts/format_check.py --root .
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors
cmake --build --preset dev
ctest --preset dev
```

Replace the configure, build, and test commands with the destination project's
equivalents when it does not use these presets.

Adoption is complete only when:

- all first-party C++ in scope follows the naming conventions;
- public API contracts are documented;
- format checking and clang-tidy pass with warnings promoted to errors;
- the project builds and its tests pass;
- serialized formats, CLI output, and import/export behavior remain intentional;
- remaining exceptions are explicit and documented rather than accidental.
