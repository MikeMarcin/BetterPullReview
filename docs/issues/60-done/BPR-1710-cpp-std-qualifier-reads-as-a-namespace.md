---
schema: "implement-rfc.card.v1"
id: "BPR-1710"
title: "C++ std:: qualifier reads as a namespace"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1710--BPR-1710--30eeb86bfcf9.json","origin":"owner request 2026-10-21 (relayed by the coordinator)","owner_note":"We should probably be able to recognize std:: as a namespace in c++ code","related_cards":["BPR-0726","BPR-1474","BPR-1559"],"related_rfcs":["RFC-BPR-0072","RFC-BPR-0081"]}
acceptance: [{"criterion":"AC1: the card names why std:: is not a namespace today","outcome":"pass","revision":"9933b54e2","evidence":{"kind":"observation","detail":"Reproduced through the public classifier at revision 8: std::vector, std::ranges::find and ::std::size_t publish one built-in span over the whole chain. The packaged C++ grammar's DispatchNamespace rule paints std:: and everything after it Standard Classes (built-in); the identifier post-pass reads only normal runs, and cpp_identifier_context knew only namespaces the blob declares, with paint() refusing to replace a built-in role.","path":"docs/issues/60-done/BPR-1710-cpp-std-qualifier-reads-as-a-namespace.md"}},{"criterion":"AC2: std, the namespaces the standard nests in it, using-directive and alias-target segments are namespaces; class scopes, undeclared qualifiers and protected regions are not; C unchanged","outcome":"pass","revision":"9933b54e2","evidence":{"kind":"command","detail":"cpp_namespace_qualifier_contract pins N01-N23 of cpp_namespace_qualifiers.cpp at exact ranges: std, ::std, std::ranges, std::chrono, std::chrono_literals, std::filesystem, alias and using-directive segments and their later uses are namespaces; std::string::npos, std::numeric_limits, std::vector and std::ranges::find keep the grammar's built-in role; Qt::AlignLeft, mystd::, a declared class and enum scope and a struct std inside a namespace are not namespaces; a directive, a comment and a string name none; C reads none. core_computation_boundaries checks the rule without Qt.","path":"tests/syntax_contract/syntax_classification_contract_tests.cpp"}},{"criterion":"AC3: tests fail before and pass after; revision raised; Qt check in Light and Dark; gates","outcome":"pass","revision":"9933b54e2","evidence":{"kind":"command","detail":"On revision-8 sources the std, alias-target and directive-use expectations fail and the controls hold; test_std_namespace_reaches_the_pane fails on std. All pass after; cpp_identifier_heuristic_revision is 9 and revision-8 keys are refused. Focused dev tests, application_cpp_identifier_appearance, core-only (311 of 313; the other two are the worktree's issue counter and .agents link), clang-format 22, clang-tidy on changed sources and the reseal checks pass.","path":"docs/issues/60-done/BPR-1710-cpp-std-qualifier-reads-as-a-namespace.md"}}]
---

# BPR-1710: C++ `std::` qualifier reads as a namespace

The owner asked on 2026-10-21: "We should probably be able to recognize
std:: as a namespace in c++ code".

## Acceptance

- **AC1, cause:** the card names why `std` is not painted as a namespace,
  against the shipped code.
- **AC2, rule:** under the C++ definition, `std` and the namespaces the
  standard nests in it are namespaces wherever they qualify a name, including
  `::std`; every segment of a using-directive and of a namespace alias's
  target is a namespace, and so are later uses through them. A class or enum
  scope, a qualifier the blob gives no evidence for and anything inside a
  directive, comment or string are not namespaces. The name after a `std`
  qualifier keeps the grammar's role. C keeps its previous roles.
- **AC3, tests:** a public-classifier test over a committed corpus fails
  before the change and passes after; a Qt check reads the namespace colour
  off a real pane in Light and Dark; the heuristic revision is raised; the
  focused suites, core-only, clang-format 22, clang-tidy on the changed
  sources and the documentation gate pass.

## Cause

The packaged C++ grammar (KSyntaxHighlighting 6.28.1, `isocpp.xml`) has a
`DispatchNamespace` rule that matches `std::` (and `boost::`) and paints it
and every following `::`-separated name in one `Standard Classes` run, which
maps to the built-in role. So `std::ranges::find` was one built-in span with
no namespace in it. The C-family post-pass in
`packaged_syntax_classifier.cpp` reads only normal runs, so it never saw the
chain. The C++ refinement (`cpp_identifier_context.cpp`, RFC-BPR-0072) knew a
namespace only from a declaration in the same blob, and its `paint` would not
replace a built-in role anyway. `using namespace std;` already painted `std`
there, because `std` without `::` is not in the grammar's chain, but it did
not record the fact, so nothing later used it.

Qualifiers that the grammar leaves normal (`foo::bar::baz()` with no
declaration in the blob) were painted as types by RFC-BPR-0072's generic
scope rule. That is still the rule. Without evidence a namespace and a class
cannot be told apart: `Foo::bar` is a class scope as often as a namespace.

## Rule

All of it is in the core refinement, `src/core/src/cpp_identifier_context.cpp`.
It stays under RFC-BPR-0081's 800-line bound (776 lines) and adds no pass
over the blob.

1. **`std` is predeclared.** `std` is the namespace the language itself
   names ([namespace.std]), and a program may not add declarations to it, so
   the namespaces nested in it form a closed set that the standard fixes.
   The refinement predeclares them in the global scope, exactly as though the
   blob declared them: `std` and, under it, `chrono`, `execution`,
   `experimental`, `filesystem`, `linalg`, `literals`, `meta`, `numbers`,
   `placeholders`, `pmr`, `ranges` (with `views`), `regex_constants`,
   `rel_ops`, `this_thread`, `views`, and the four inline literal namespaces
   under both `std` and `std::literals`. This is the one fixed list. It is
   the standard's own list, not a guess, but it is a list, and it is limited
   to `std`.
2. **A using-directive and an alias target name namespaces.** Only a
   namespace can be named there, and a class cannot enclose a namespace, so
   every segment of `using namespace a::b;` and of the target in
   `namespace x = a::b;` is recorded as a namespace. A segment the blob
   already knows keeps its identity: a namespace is entered, and a class or
   shadow ends the record rather than being overridden.
3. **Uses resolve through the ordinary lookup.** A qualified chain whose head
   resolves to a namespace (a declaration, a directive, an alias target or
   the predeclared `std`) paints that segment, and each further namespace
   segment, as a namespace. A namespace may replace the grammar's built-in
   role, where an inferred role still may not; no language type can precede
   `::`, so no other built-in token can resolve as one. A `::` that the
   grammar's built-in chain covered becomes the operator every other
   qualified name's separator already is.

Edge cases:

- **Class scope under `std`.** `std::string::npos`: `string` is not a
  namespace the standard declares, so it and `npos` keep the grammar's
  built-in role. The same holds for `std::chrono::system_clock::now()` past
  `chrono`, and for `std::numeric_limits<int>::max()`.
- **A blob that declares `std` itself.** `namespace lib { struct std {}; auto
  v = std::x; }` resolves `std` to the class inside `lib`, so it is not a
  namespace there. `namespace std { … }` merges with the predeclared one.
- **Class and enum scopes the blob declares** keep their type and
  enum-member roles (`Widget::Mode::Fast`).
- **Undeclared qualifiers** keep RFC-BPR-0072's type role (`foo::bar::baz()`
  without a declaration, `mystd::thing()`).
- **`Qt::`** is unchanged: the grammar's Qt extension lists `Qt` among its
  Qt classes and paints `Qt::AlignLeft` built-in, and the blob gives no
  evidence. Recognising `Qt`, `boost` or any other library namespace would
  take a second fixed list, which is the owner's call.
- **Unqualified uses after a directive** (`using namespace std; chrono::seconds`)
  are not followed: the refinement does not model the names a directive
  makes visible.

C and Windows Resource do not run the C++ refinement and keep their roles.
`cpp_identifier_heuristic_revision` is 9, so spans cached under revision 8
are refused. No new role, colour, grammar or library: the existing
`syntax_namespace` role and its palette entries are used as they are.

This amends two expectations: `std::wstring` (BPR-0726) and
`std::make_unique` (BPR-1559) used to be pinned as one built-in span. `std`
is now a namespace there and the name after it stays built-in. The owner's
request is the decision for that change; the renders below are for the
owner to see it.

## Evidence

Implementation `9933b54e2`, rebased on main `0b8d8a93f`; macOS (Apple M4 Pro),
2026-10-21, `dev` built with `-g0` to save disk. The focused suites, the
composed public path and the renders were rerun after the rebase.

- AC1: with the revision-8 sources, the contract's softened run reports the
  `std::vector`, `::std`, `std::chrono_literals`, `std::ranges`,
  `std::chrono`, `std::numeric_limits`, `std::string`, alias-target and
  directive-use expectations failing; `using namespace std` and every
  negative control already held.
- AC2 and AC3: `cpp_namespace_qualifier_contract` in
  [`syntax_classification_contract_tests.cpp`](../../../tests/syntax_contract/syntax_classification_contract_tests.cpp)
  classifies
  [`cpp_namespace_qualifiers.cpp`](../../../tests/fixtures/syntax/cpp_namespace_qualifiers.cpp)
  through the public classifier, as described in AC2. `test_namespace_qualifiers`
  in
  [`core_computation_boundaries_tests.cpp`](../../../tests/core/core_computation_boundaries_tests.cpp)
  checks the rule with no grammar and no Qt.
  `test_std_namespace_reaches_the_pane` in
  [`syntax_pane_composition_tests.cpp`](../../../tests/adapters_qt/syntax_pane_composition_tests.cpp)
  publishes a C++ comparison to a real `diff_workbench` and reads the painted
  foreground off the block layout in Light and Dark: `std` and `ranges` in
  the namespace colour and `find` in the built-in colour. On revision 8 it
  fails at "std is painted in the namespace colour". With
  `BPR_STD_NAMESPACE_RENDER_ROOT` set it saves the composed workbench, which
  is how the 2x before and after renders were made (kept outside the
  repository).
- Suites: `syntax_classification_contract`, `syntax_refinement_cancellation`,
  `qt_syntax_pane_composition`, `core_computation_boundaries`,
  `architecture_boundaries`, `architecture_boundary_contract` and
  `application_cpp_identifier_appearance` pass. `core-only` builds and passes
  311 of 313; `documentation_structure` fails only because this worktree's
  history holds another branch's issue counter (`BPR-1709 rewinds historical
  high water BPR-1710`), and `rfc0016_production_identity_security` fails
  only while the worktree carries an `.agents` link and passes without it.
  `coding-format-check` (clang-format 22) passes, clang-tidy on the changed
  sources passes, and both reseal checks report nothing to reseal.
- The RFC-BPR-0081 evaluation harness (`BPR_BUILD_SYNTAX_EVALUATION`) was not
  re-run. Its contract twins compile this contract against the tree-sitter
  candidate, and the new and amended `std` expectations were not checked
  against that candidate. The evaluation is closed and nothing ships from it.
