---
schema: "implement-rfc.card.v1"
id: "BPR-1559"
title: "C++ template calls and template arguments stay uncoloured"
status: "Ready"
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
