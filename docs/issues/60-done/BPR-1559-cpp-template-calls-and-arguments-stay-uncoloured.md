---
schema: "implement-rfc.card.v1"
id: "BPR-1559"
title: "C++ template calls and template arguments stay uncoloured"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-16 (relayed by the coordinator)","owner_note":"our syntax highlighting doesn't understand C++ template functions. `getComponent<Type>()` colors neither the function name nor the template type arg.","related_cards":["BPR-0726","BPR-1474"],"related_rfcs":["RFC-BPR-0072","RFC-BPR-0081"],"reservation":"docs/issues/_reservations/BPR-1559--BPR-1559--472ded6f2d78.json"}
acceptance: [{"criterion":"AC1: the card names exactly why getComponent<Type>() paints neither word","outcome":"pass","revision":"ff7b83ea2","evidence":{"kind":"observation","detail":"Reproduced through the public classifier at revision 7: no span for getComponent or Type. Cause: the packaged isocpp.xml grammar styles user identifiers as normal text with no template rule; identifier_role named a function only when ( directly followed the name and a type only for PascalCase followed by a name, &, * or <; cpp_identifier_context paints only namespace, enum and :: chains. Tree-sitter is not shipped (RFC-BPR-0081 rejected it).","path":"docs/issues/60-done/BPR-1559-cpp-template-calls-and-arguments-stay-uncoloured.md"}},{"criterion":"AC2: template calls, arguments and template types classify under C++; comparisons stay unpainted; C unchanged","outcome":"pass","revision":"007b39788","evidence":{"kind":"command","detail":"cpp_template_contract pins T00-T04, T10-T24 and T30-T34 at exact ranges (function for free, ., ->, ns:: and template-disambiguated calls, declarations and specializations; type for qualified, nested, pointer/reference/cv and function-type arguments and instantiated types; number, macro, builtin, std:: and enumerator roles kept), nine comparison/shift lines with no function or type role, and C keeping getComponent, Type and T normal.","path":"tests/syntax_contract/syntax_classification_contract_tests.cpp"}},{"criterion":"AC3: golden test fails before and passes after with negative control; revision raised; Qt render; suites, format, tidy and docs gate","outcome":"pass","revision":"007b39788","evidence":{"kind":"command","detail":"Contract and test_template_call_roles_reach_the_pane fail on revision-7 sources and pass after; revision-7 keys refused (cpp_identifier_heuristic_revision 8). Full dev run once (739 tests, staged for disk): 728 passed; of 11 load/staging failures 9 pass alone or after merge, application_large_review_launch misses a process-start budget before classification, live_mutation_isolation cannot run on a batched e2e tree. Focused syntax, Qt, appearance and documentation rows pass at 007b39788; coding-format-check, tidy on changed sources (no new findings), documentation gate and its tests pass.","path":"docs/issues/60-done/BPR-1559-cpp-template-calls-and-arguments-stay-uncoloured.md"}},{"criterion":"AC4: other generic languages recorded","outcome":"pass","revision":"007b39788","evidence":{"kind":"observation","detail":"TypeScript, Rust, C#, Java, Swift, Kotlin and Go measured through the same classifier; each is its own upstream grammar with no post-pass, so the C++ layer cannot reach them. TypeScript and C# share the type-argument gap (C# also free generic calls); Rust, Swift, Kotlin and Go colour no calls at all.","path":"docs/issues/60-done/BPR-1559-cpp-template-calls-and-arguments-stay-uncoloured.md"}}]
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

## Evidence

Implementation `ff7b83ea2` and `972819bb1`, merged with main at `007b39788`;
macOS (Apple M4 Pro), 2026-10-16.

- AC1: the root cause above was reproduced through the public classifier
  before any change: on `auto a = getComponent<Type>();` revision 7
  published no span for either word (only `auto` as a keyword).
- AC2 and AC3: `cpp_template_contract` in
  [`syntax_classification_contract_tests.cpp`](../../../tests/syntax_contract/syntax_classification_contract_tests.cpp)
  classifies the committed corpus
  [`cpp_templates.cpp`](../../../tests/fixtures/syntax/cpp_templates.cpp)
  and pins every form at its exact range: free, `.`, `->`, qualified and
  `template`-disambiguated calls (T10 to T14), built-in, qualified, nested
  (`>>>`), pointer/reference/cv, non-type (number, ALL_CAPS) and function-type
  arguments, a cast, a `std::` call, a known enumerator argument (T15 to T24),
  declarations, an out-of-line definition and an explicit specialization (T00
  to T04), and instantiated types including `std::vector<std::pair<Key,
  Value>>` and a lower-case template (T30 to T34). Its negative control holds
  nine comparison and shift lines (`a < b > c`, `i<n`, `a<b && c>d`,
  `(a<b) + (c>d)`, `a<b || c>d`, `a<<b`, `a<=b`) with no function or type
  role, and classifying the same corpus as C keeps `getComponent`, `Type` and
  a lone `T` normal. A key carrying revision 7 is refused. With the
  revision-7 sources restored the contract fails at its first expectation
  (the owner's case) and passes at `ff7b83ea2`.
  `test_template_call_roles_reach_the_pane` in
  [`syntax_pane_composition_tests.cpp`](../../../tests/adapters_qt/syntax_pane_composition_tests.cpp)
  classifies `getComponent<Type>()`, publishes it to a real `diff_workbench`
  on the light profile and reads the painted foreground off the block layout:
  the function colour on `getComponent`, the type colour on `Type`. It fails
  on the revision-7 sources ("the template call's name is painted as a
  function") and passes after.
- Cost: the scan is one linear pass per C++ line, at most 5 bytes per unit of
  the longest line. Interleaved timings against revision 7 on 1 MiB
  single-line inputs (`a<` repeated, nested `a<…>`, mixed lists and
  comparisons) are within 3%, and an 8 MiB realistic file is unchanged. A
  randomized ASan and UBSan harness of 300,000 lines, querying every range
  including out-of-bounds ones, reported nothing.
- Suites at `007b39788` (`dev`, built with `-g0` for disk space; the build
  type and tests are unchanged): `syntax_classification_contract`,
  `syntax_refinement_cancellation`, `qt_syntax_pane_composition`,
  `application_cpp_identifier_appearance`, `application_appearance_host`,
  `qt_composed_github_scale_100` and `documentation_structure` pass.
  The full `dev` suite was run once at the first merge with main (739 tests,
  in stages because a complete `dev` tree is about 9 GB and the disk was
  nearly full): 728 passed and 11 failed under parallel load or staging.
  Rerun alone, `application_diff_host`,
  `qt_composed_inline_drawer_scale_200`,
  `qt_composed_long_thread_scale_100`, `qt_hide_drawers_native_menu_key`,
  `application_review_tour_publication_reconciliation`,
  `application_diagnostic_subsystems` and `application_private_review_native`
  passed. `rfc0016_production_identity_security` failed only because of the
  worktree's `.agents` link and passed without it. `documentation_structure`
  was behind main's issue counter and passed after the merge.
  `application_large_review_launch` failed on its process-to-main start-up
  budget (1,031 ms against 100 ms) on a loaded machine, before any
  classification runs. `live_mutation_isolation` could not run because it
  reads every end-to-end binary at once and those were built and deleted in
  batches. The change adds no network, process or mutation code.
  `coding-format-check` (clang-format 22) passes; clang-tidy on the changed
  sources reports only two existing naming notes in the Windows resource
  contract. The documentation gate and its 67 tests pass.
- The RFC-BPR-0081 evaluation harness (`BPR_BUILD_SYNTAX_EVALUATION`) was not
  re-run. It scores namespace, enum and macro roles, which this change does
  not paint, and `cpp_identifier_context.cpp` is unchanged.
- No new role or colour, so the owner is not asked to judge one; the function
  and type colours are the ones BPR-0726 and BPR-0738 settled.
