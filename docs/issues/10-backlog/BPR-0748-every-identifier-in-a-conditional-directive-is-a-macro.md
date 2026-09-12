---
schema: "implement-rfc.card.v1"
id: "BPR-0748"
title: "Should every identifier in a conditional directive take the macro purple?"
status: "Backlog"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0744"]
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"carried_from":"BPR-0744","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"4f1d1354-c3c1-43d9-af11-358347147453","parked_question":"human-review"}
---

# Should every identifier in a conditional directive take the macro purple?

This is a question for the owner, not a defect. It was costed on [[BPR-0744]]
and deliberately not taken, it rode BPR-0744's v009 candidate as the one new
question that candidate asked, and it came back unanswered: the owner accepted
`BPR-0744-directive-ranges` (decision `4f1d1354`, 2026-09-12T16:06:15Z) **with
an empty note**. An accept of the implemented behaviour is not an answer to a
question about behaviour that was not implemented, so the question is carried
here rather than read into the accept or dropped with it.

## The question, as it was put to him

A macro is recognised by ALL_CAPS, so

    #if defined(__GNUC__) && !defined(__clang__) && __cplusplus >= 202002L

paints `__GNUC__` purple and `__clang__` and `__cplusplus` grey. Inside `#if`,
`#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()` every identifier is a
macro by the language's own rules, so all of them could be purple.

## Why it is a real question and not an obvious yes

The rule is exact. [cpp.cond]/4: after replacement, every remaining identifier
in an `#if` or `#elif` controlling expression is replaced by `0`, so it either
was a macro or is being tested as one; in `#ifdef`, `#ifndef` and `#undef` the
single identifier is a macro by definition.

But it puts **more** colour on a directive line than the owner's own reference
screenshot shows. His complaint through v005 to v008 was the opposite — too
little colour, directives grey end to end — so widening past what the reference
draws is a visible change to a shape he has not seen and has not asked for. That
is why BPR-0744 costed it and left it.

## What it costs, from BPR-0744's own costing

- About 35 lines of rule. The exceptions that must stay grey are `defined`,
  `true`, `false` and the `__has_include` / `__has_cpp_attribute` /
  `__has_builtin` operator family. `#define` must stay excluded, because its
  replacement tokens are not macros.
- The work is not the rule but the state. A directive's run is split by the
  grammar (`#if defined(` … `__APPLE__` … `) && (defined(`), so a per-run
  keyword lookup sees only the first run. It needs one per-line member
  recording which keyword opened the line, set on the line's first preprocessor
  run and cleared in `begin_line`, which already resets per line.
- Plus about four fixture lines, five assertions and a revision bump.
- It is confined to directive lines and leaves ordinary code untouched.

## What it would fix

The two limits [[BPR-0744]] pinned as facts rather than leaving to drift, both
visible on the lines the owner reads:

- listed upstream but lowercase → `syntax_other` `B1BAC4`: `__cplusplus`,
  `__linux__`, `__linux`.
- not listed upstream and lowercase → `syntax_preprocessor` `A3A3A3`:
  `__clang__`, `__xlC__`, `__xlc__`.

`is_macro_case` was not widened instead, and must not be: accepting
all-lowercase-with-underscores would sweep up `text`, `aValue` and every
ordinary snake_case identifier in a normal run.

## Scope

- Ask the owner, on a bundle, with the two shapes side by side rather than in
  words: the same header as it paints today and as it would paint under the
  rule. He has answered every colour question from a picture and none from a
  description.
- Implement only if he says yes. If he says no, record the answer and delete
  this card's question rather than leaving it open for a fourth packet.
- Owned paths if it is taken: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`,
  `src/adapters/syntax/include/bpr/adapters/syntax/packaged_syntax_classifier.hpp`,
  `tests/syntax_contract/syntax_classification_contract_tests.cpp` with
  `tests/fixtures/syntax/identifier_roles.cpp`. No palette value moves and no
  appearance role is added, so there is no schema move and no digest repin.

## Acceptance

- The owner answers the question against two renderings of the same header, in
  an attended session, and the answer is recorded here and on RFC-BPR-0006.
- If yes: `syntax_classification_contract` pins, on one fixture line of the
  shape above, that `__clang__` and `__cplusplus` take `syntax_macro` while
  `defined`, `true`, `false` and the `__has_*` family stay
  `syntax_preprocessor`, and that a `#define`'s replacement tokens are
  unaffected. `qt_syntax_pane_composition`, `qt_appearance_*` and
  `core_appearance_contrast` pass.
- If no: the two limits above are restated on [[BPR-0744]] as the settled
  answer rather than as pinned accidents, and this card is closed.

## Not asked again by accident

BPR-0744's v009 candidate carried this question in its prose. The next packet
that carries it must carry it as **its own candidate with its own criterion**,
not as a rider on a criterion about something else: a rider is exactly what
produced this empty-note accept, and before it the owner's "As I answered in
another question" on v008.
