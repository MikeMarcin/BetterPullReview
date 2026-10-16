---
schema: "implement-rfc.card.v1"
id: "BPR-1559"
title: "C++ template calls and template arguments stay uncoloured"
status: "In Progress"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-16 (relayed by the coordinator)","owner_note":"our syntax highlighting doesn't understand C++ template functions. `getComponent<Type>()` colors neither the function name nor the template type arg.","related_cards":["BPR-0726","BPR-1474"],"related_rfcs":["RFC-BPR-0072","RFC-BPR-0081"],"reservation":"docs/issues/_reservations/BPR-1559--BPR-1559--472ded6f2d78.json"}
---

# BPR-1559: C++ template calls and template arguments stay uncoloured

The owner reported on 2026-10-16: "our syntax highlighting doesn't understand
C++ template functions. `getComponent<Type>()` colors neither the function
name nor the template type arg."

The shipped classifier is KSyntaxHighlighting's packaged C++ grammar plus the
C-family line heuristic of [[BPR-0726]] (`identifier_role` in
`src/adapters/syntax/src/packaged_syntax_classifier.cpp`) and the C++
namespace and enum refinement of
[RFC-BPR-0072](../../rfcs/RFC-BPR-0072-cpp-identifier-context.md)
(`cpp_identifier_context.cpp`). Tree-sitter is not shipped:
[RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md)
evaluated and rejected it. The grammar styles every user identifier as normal
text and has no template rule, so a template call or argument gets a role only
if the heuristic names it.

## Acceptance

- **AC1, root cause:** the card names exactly why `getComponent<Type>()`
  paints neither word, against the shipped code.
- **AC2, forms:** under the C++ definition, a function name followed by a
  template argument list and then `(` is a function, for free, `.`, `->`,
  qualified and `template`-disambiguated calls, declarations, out-of-line
  definitions and explicit specializations; a template argument in type
  position is a type, including qualified, nested and pointer or reference
  arguments; a template name used as a type (`Foo<Bar> x;`,
  `Factory<Widget>::create()`) is a type; numbers, ALL_CAPS constants and
  the grammar's own roles (keywords, `std::` names, built-in types) keep their
  roles. Comparisons and shifts are not painted as templates, and C keeps its
  previous roles.
- **AC3, tests:** a public-classifier golden test over a committed C++ corpus
  holding every form above fails before the change and passes after, with a
  negative control of comparisons; the C++ heuristic revision is raised so
  spans cached under the old rules are refused; one Qt render of a sample
  shows the roles painted; the existing syntax contracts, the full `dev`
  test suite, clang-format 22, tidy on changed lines and the documentation
  gate pass.
- **AC4, other languages:** the card records whether TypeScript, Rust, C#,
  Java, Swift and Kotlin have the same gap and whether the same layer can fix
  it.

## Root cause

Three facts together, each in the shipped code at `f721b18b4`:

1. The packaged C++ grammar (KSyntaxHighlighting `isocpp.xml`) styles every
   user identifier as `Normal Text` and `<`/`>` as `Symbol`; it has no rule
   for a template argument list. Only `std::`-qualified names (Standard
   Classes), `*_t` names and keywords get roles of their own.
2. The C-family line heuristic `identifier_role` names a function only when
   the next non-blank unit after the name is `(`. In `getComponent<Type>()`
   the next unit is `<`, so `getComponent` stayed normal. Its type rule
   needs a PascalCase word followed by a name, `&`, `*` or `<`; `Type` is
   followed by `>`, so it stayed normal too. A lower-case template name
   (`object_pool<Element> pool;`) and a single-capital parameter (`T get();`)
   were never types.
3. The C++ refinement (`cpp_identifier_context.cpp`) pairs template angles
   only to protect enumerator boundaries and paints only namespace, enum and
   `::`-chain roles, so nothing later repaired the two words.

The premise that highlighting is tree-sitter based does not hold:
tree-sitter is built only by the RFC-BPR-0081 evaluation harness
(`BPR_BUILD_SYNTAX_EVALUATION`), which rejected it on 2026-10-13. There are
no packaged highlight queries.

## Implementation

`src/adapters/syntax/src/cpp_template_arguments.{hpp,cpp}` scans each C++
line once, linearly, before the grammar reports its runs. A `<` opens a list
only directly after a name (not `operator`), not as `<<` or `<=`, and only if
a `>` at the same parenthesis depth closes it on the same line with nothing
between that a template argument list cannot hold at that depth (`;`, a
brace, a string, `=`, `?`, `|`, `->`, a logical `&&`, another `<`
comparison, a parenthesis closing one opened before the list). `identifier_role`
asks it, for C++ only and after the literal-prefix, `class`/`typename` and
ALL_CAPS macro rules:

- a name whose list is followed by `(` is a function (free, `.`, `->`,
  `ns::`, `template`-disambiguated calls; declarations; specializations);
- a name whose list is followed by a name, `::`, `&`, `*` or `{`, or which is
  itself an argument, is a type (`Foo<Bar> x;`, `Factory<Widget>::create()`);
- an argument is a type when it alone fills its argument after leading
  qualifiers and a `::` path: `<` or `,` before it, and `>`, `,`, `(`, `[`,
  `...` or trailing `*`, `&`, `const`, `volatile` after it.
- a single capital is a type where a PascalCase word would be.

Numbers, ALL_CAPS constants, keywords, built-in types and `std::` names keep
the grammar's or the macro rule's role; a known enumerator in an argument is
still repainted by the C++ refinement. C and Windows Resource do not run the
scan and keep their roles and cache identity. `cpp_identifier_heuristic_revision`
is 8. No new role, colour or library; `cpp_identifier_context.cpp` is
untouched (RFC-BPR-0081's 800-line and no-new-pass rules hold).

Known limits, all leaving words normal rather than miscoloured: a list that
spans lines; a parameter inside a function type's parentheses
(`std::function<R(Argument)>` names `R` but not `Argument`); a type
argument after a nested list's `::` (`Foo<Outer<T>::type>`); and a
non-type argument written as a lone lower-case or PascalCase name
(`std::array<int, size>`) is a type, as C++'s own type-id-first rule and
tree-sitter-cpp read it.

## Other languages

Measured through the same public classifier on 2026-10-16. None of these
definitions runs a post-pass; each is its own upstream grammar, so the C++
fix cannot reach them and a fix would be a new per-language heuristic, not
this layer.

| Language | `f<T>()` / generic call | Type argument | Plain call `f()` |
|---|---|---|---|
| TypeScript | function | normal | function |
| Rust (`f::<T>()`) | `f::` painted as the grammar's scope (preprocessor) role | normal | normal |
| C# | free call normal; `obj.F<T>()` function | normal | function |
| Java (`Util.<T>f()`) | function | builtin only for library classes | function |
| Swift | normal | normal | normal |
| Kotlin | normal | normal | normal |
| Go (`F[T]()`) | normal | normal | normal |

TypeScript and C# share the C++ gap for type arguments (C# also for free
generic calls); Rust, Swift, Kotlin and Go colour no calls at all, so their
gap is general rather than generic-specific.
