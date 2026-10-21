---
schema: "implement-rfc.card.v1"
id: "BPR-1711"
title: "C++ std after -> reads as a namespace"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1711--BPR-1711--4d1d526f06b6.json","origin":"owner review of BPR-1710's after-render, 2026-10-21 (relayed by the coordinator)","owner_note":"Line 5, auto load(const fs::path& aPath) -> std::vector<std::string>: the std after -> keeps the built-in colour while the std inside <...> is a namespace. Why?","related_cards":["BPR-1710","BPR-1559"],"related_rfcs":["RFC-BPR-0072","RFC-BPR-0081"]}
acceptance: [{"criterion":"AC1: the card names why the std after -> stayed built-in","outcome":"pass","revision":"916adaaa3","evidence":{"kind":"observation","detail":"Reproduced through the public classifier at revision 9: in auto load(...) -> std::vector<std::string> the leading std::vector is one built-in span and the inner std is a namespace. The grammar spans are not the cause: isocpp.xml reads - and > as ordinary Symbol characters and its DispatchNamespace rule fires the same way after them. cpp_identifier_context's classify_chain skipped all lookup for a chain whose head follows . or ->, treating it as member access, and a trailing return type shares the -> token.","path":"docs/issues/60-done/BPR-1711-cpp-std-after-arrow-reads-as-a-namespace.md"}},{"criterion":"AC2: std after ->, in a lambda's and a compound requirement's ->, after real member access and in a qualified class head is a namespace; member access without a qualifier and class scopes after member access are unchanged; the other contexts are pinned","outcome":"pass","revision":"916adaaa3","evidence":{"kind":"command","detail":"cpp_namespace_qualifier_contract pins N24-N36 of cpp_namespace_qualifiers.cpp at exact ranges: std after a trailing ->, a lambda's ->, [] ->, a compound requirement's ->, p->std:: and e.std:: is a namespace and the standard name keeps the built-in role; pointer->field stays normal; pointer->Widget::count() keeps Widget a type and object.Widget::Mode::Fast resolves no enumerator; template <> struct std::hash<Widget> keeps std a namespace in the whole blob and struct foo::bar::Gadget keeps foo and bar namespaces; std after (, ,, <, a cast, decltype(, requires, a template's >, noexcept(, throw, co_return and return is a namespace. core_computation_boundaries checks the rule without Qt.","path":"tests/syntax_contract/syntax_classification_contract_tests.cpp"}},{"criterion":"AC3: tests fail before and pass after; revision raised; Qt check in Light and Dark; gates","outcome":"pass","revision":"916adaaa3","evidence":{"kind":"command","detail":"On revision-9 sources every new -> and . expectation and the specialization expectation fail and the member-access controls hold; core_computation_boundaries and test_std_namespace_reaches_the_pane fail on the new checks. All pass after; cpp_identifier_heuristic_revision is 10 and revision-9 keys are refused. Focused dev suites, application_cpp_identifier_appearance, core-only, clang-format 22, clang-tidy on the changed sources and both reseal checks pass.","path":"docs/issues/60-done/BPR-1711-cpp-std-after-arrow-reads-as-a-namespace.md"}}]
---

# BPR-1711: C++ `std` after `->` reads as a namespace

The owner looked at [[BPR-1710]]'s after-render and asked about line 5,
`auto load(const fs::path& aPath) -> std::vector<std::string>`: the `std`
right after `->` kept the grammar's built-in colour, while the `std` inside
`<…>` had the namespace colour. Why?

## Acceptance

- **AC1, cause:** the card names why that `std` stayed built-in, against the
  shipped code.
- **AC2, rule:** a qualifier followed by `::` resolves as a namespace after
  `->`, whether `->` begins a trailing return type (of a function, a lambda
  or a compound requirement) or is real member access, and after `.`.
  Member access without a qualifier (`obj->field`) is unchanged, and so is a
  class scope after member access. Sibling misses in the same family are
  found and fixed, and the contexts that already worked are pinned.
- **AC3, tests:** the public-classifier contract and the Qt pane check fail
  before the change and pass after; the heuristic revision is raised; the
  focused suites, core-only, clang-format 22, clang-tidy on the changed
  sources and the documentation gate pass.

## Cause

The grammar spans were not the cause. The packaged C++ grammar
(KSyntaxHighlighting 6.28.1, `isocpp.xml`) reads `-` and `>` as ordinary
`Symbol` characters in `Main`, and its `DispatchNamespace` rule paints
`std::vector` as one built-in run after them as it does anywhere else.

The cause was in the C++ refinement, `src/core/src/cpp_identifier_context.cpp`.
`classify_chain` did no lookup at all for a qualified chain whose head
followed `.` or `->`, so that `object.GCT::Network::Reason::ID` would not
resolve `ID` as an enumerator: after member access the last segment names a
member. But `->` also begins a trailing return type, and the token is the
same. So `-> std::vector` never reached the predeclared `std`, and the
grammar's built-in role stayed. `<std::string>` follows `<`, so it resolved.

## Sibling misses

A probe classified `std::` after each of `.`, `::`, `,`, `(`, `<`, `return`,
`throw`, `co_return`, a cast, `decltype(`, `noexcept(`, `requires`, the start
of a line, a template's `>`, a lambda's `->`, a ternary, range-`for`, a fold,
a pack, `new`, `delete`, a base clause and a deduction guide.

- **Same cause:** a lambda's `-> std::string`, a parameterless lambda's
  `[] -> std::size_t`, a compound requirement's `{ e } -> std::convertible_to<…>`,
  and real member access through a namespace, `p->std::exception::what()`
  and `e.std::exception::what()`. The last two are valid C++: they name a
  base-class member.
- **A second miss:** `template <> struct std::hash<Key> {` and other
  qualified class heads. `enum_or_type_declaration` took the first segment,
  `std`, as the declared name and declared it a class in the enclosing
  scope. That clashed with the predeclared namespace, which became
  `unknown`. Declarations are collected over the whole blob before uses are
  classified, so one specialization left no `std` anywhere in the file a
  namespace, lines before it included. `struct foo::bar::Gadget { }` did the
  same to a namespace the blob declares.
- Every other context listed above already resolved.

## Rule

Both changes are in the core refinement. The file is 788 lines, under
RFC-BPR-0081's 800-line bound, and no pass over the blob is added.

1. **After `.` or `->`, only a namespace resolves.** The chain is looked up
   as any other qualified name, and a result is kept only while it is a
   namespace or a namespace alias. A class declares no namespace, so a
   namespace there either qualifies a base (`p->std::exception::what()`) or
   begins a trailing return type. The first class, enum or unknown symbol
   ends resolution, exactly as the skipped lookup did before, so
   `object.Widget::Mode::Fast` still resolves no enumerator.
2. **A qualified class head declares nothing.** In `struct a::b::C {` the
   class name is the last segment. It is painted a type, and its body opens
   its own scope. The qualifier segments are not declarations; they are
   resolved as uses, so `std` stays the namespace and `foo::bar` stay the
   namespaces the blob declares.

Edge cases:

- **`obj->field`** has no `::` after it and is not a qualified chain, so it
  is unchanged.
- **`pointer->Widget::count()`**: `Widget` is a class, so it keeps the
  generic scope's type role and is not a namespace.
- **A trailing return type through a class** (`auto f() -> Widget::Mode`)
  is not changed by this card. Its last segment keeps the member-access
  treatment and gets no type role, because a trailing `->` cannot be told
  from member access by its token alone (`f() -> T` and `f()->x` both
  follow `)`). Namespaces are unambiguous in both readings; classes are not.

C and Windows Resource do not run the C++ refinement and keep their roles.
`cpp_identifier_heuristic_revision` is 10, so spans cached under revision 9
are refused. No role, colour, grammar or library is added. No existing
contract expectation changes: `object.GCT::Network::Reason::ID` now paints
`GCT` and `Network` as the namespaces they are, and `ID` stays neutral as
pinned.

## Evidence

Implementation `916adaaa3`, on main `c7f499fe1`; macOS (Apple M4 Pro),
2026-10-21, `dev` and `core-only` built with `-g0` to save disk.

- AC1: the probe at revision 9 (outside the repository) published
  `std::vector` built-in at `-> std::vector` and `std` as a namespace at
  `<std::string>` on the same line.
- AC2 and AC3: `cpp_namespace_qualifier_contract` in
  [`syntax_classification_contract_tests.cpp`](../../../tests/syntax_contract/syntax_classification_contract_tests.cpp)
  classifies
  [`cpp_namespace_qualifiers.cpp`](../../../tests/fixtures/syntax/cpp_namespace_qualifiers.cpp)
  through the public classifier, as described in AC2. With the revision-9
  sources, a run that reports every expectation instead of stopping fails
  N24-N29 and N33. Because the N33 specialization poisons `std` for the
  whole blob, it also fails every other `std` line (N07-N11, N20, N35, N36),
  and N12 and N34 through `struct foo::bar::Gadget`. The member-access
  controls N30-N32, the directive, alias and protected-region expectations
  hold. All pass on revision 10. `test_namespace_qualifiers` in
  [`core_computation_boundaries_tests.cpp`](../../../tests/core/core_computation_boundaries_tests.cpp)
  checks the trailing return type, the specialization and `p->field` with
  no grammar and no Qt; it fails at revision 9 on the specialization.
  `test_std_namespace_reaches_the_pane` in
  [`syntax_pane_composition_tests.cpp`](../../../tests/adapters_qt/syntax_pane_composition_tests.cpp)
  now also reads the painted foreground of both `std`s on the owner's line
  in Light and Dark; at revision 9 it fails at "a trailing return type's std
  is painted like the std inside it". With `BPR_STD_NAMESPACE_RENDER_ROOT`
  set and `QT_SCALE_FACTOR=2` it saved the 2x before and after renders,
  kept outside the repository.
- Suites: `syntax_classification_contract`, `syntax_refinement_cancellation`,
  `qt_syntax_pane_composition`, `core_computation_boundaries`,
  `architecture_boundaries`, `architecture_boundary_contract` and
  `application_cpp_identifier_appearance` pass. `coding-format-check`
  (clang-format 22) and clang-tidy on the changed sources pass, and both
  reseal checks report nothing to reseal.
- `core-only` builds and passes 310 of 313. `documentation_structure`,
  `documentation_issue_graph_contract` and `issue_id_allocation_contract`
  cannot import `board_model` because the worktree has no `.agents` link.
  With a temporary link, the last two pass. `documentation_structure` and
  the documentation gate then report only this card's provisional ID. With
  a numeric stand-in, tried and reverted, they report only the issue
  counter, which allocation settles.
- The RFC-BPR-0081 evaluation harness (`BPR_BUILD_SYNTAX_EVALUATION`) was
  not re-run, as for [[BPR-1710]]: the evaluation is closed and nothing
  ships from it.
