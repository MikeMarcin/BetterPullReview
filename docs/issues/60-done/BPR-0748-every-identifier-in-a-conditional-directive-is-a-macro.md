---
schema: "implement-rfc.card.v1"
id: "BPR-0748"
title: "Should every identifier in a conditional directive take the macro purple?"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0744"]
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"carried_from":"BPR-0744","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"4f1d1354-c3c1-43d9-af11-358347147453","parked_question":"human-review"}
acceptance: [{"criterion":"AC1 owner answer on conditional macro rule","outcome":"pass","revision":"a8b0394a","evidence":{"kind":"observation","detail":"Mandate r16 records direct owner YES to BPR-0748: #if/#ifdef/#ifndef/#elif/#undef/defined() macro identifiers purple regardless of case, with operator/literal/header/attribute/builtin-test operands and #define replacement boundaries. The direct answer superseded the planned side-by-side attended question; original method remains documented on the card."}},{"criterion":"AC2 conditional roles and unchanged boundaries","outcome":"pass","revision":"a8b0394a","evidence":{"kind":"command","detail":"Red before fix: build/logs/bpr-0748-d25-red.log (__cplusplus); independent review counterexamples red before correction: bpr-0748-d25-correction-red.log (comment-separated builtin operand and continued #if). Final syntax_classification_contract plus qt_syntax_pane_composition, qt_appearance_settings_panel/profile, core_appearance_contrast all 5/5 pass: bpr-0748-d25-correction-focused.log. Combined syntax/seam 9/9 at final tree: d25-syntax-seam-integration.log. Format/tidy pass in correction logs."}},{"criterion":"AC3 no generalized identifier or define broadening","outcome":"pass","revision":"a8b0394a","evidence":{"kind":"observation","detail":"Fixture asserts ordinary_name in #define replacement is not syntax_macro, and the classifier gates lowercase promotion on a recognized conditional directive and named predefined-macro formats. No palette/schema role changed; heuristic revision 3 retires cached spans."}}]
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

## Owner answer and implemented scope, 2026-09-12 (d25, mandate r16)

The owner answered **yes** to this card's question directly in chat and fixed
its bounds: identifiers in `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and
`defined()` take macro purple regardless of capitalization. Operators,
literals, header names, attributes and builtin-test operands retain their
roles. Ordinary identifiers and all `#define` replacement tokens do not
become macros. The direct answer supersedes the earlier plan to obtain the
same yes/no answer through a side-by-side attended candidate; that plan is
retained above as the historical reason the question existed. No second owner
answer is sought.

The syntax owner reproduced `__cplusplus` as `syntax_other` before the fix
(`build/logs/bpr-0748-d25-red.log`), then added per-line directive context and
operator/operand exclusions. The new fixture covers mixed-case directives,
lowercase names, `defined`, boolean and alternative operators, builtin-test
operands, character literals, header tests and unaffected `#define`
replacement text. The heuristic revision advances from 2 to 3 so cached
spans are retired. Focused build, five required contracts, format and tidy
pass in `build/logs/bpr-0748-d25-focused-build.log`,
`bpr-0748-d25-focused.log`, `bpr-0748-d25-format.log` and
`bpr-0748-d25-tidy.log`. These local logs are evidence pointers, not committed
artifacts. The integrator's final acceptance/commit follows separately.

Independent review then found two counterexamples within that same accepted
rule: a comment between `__has_cpp_attribute` and its operand could make
`nodiscard` purple, and a backslash-continued `#if` could leave a lowercase
macro grey. Both assertions failed before correction
(`build/logs/bpr-0748-d25-correction-red.log`). The syntax owner kept
conditional and builtin-operand state across continued physical lines and
treated the separating comment as operand trivia. The final focused build,
all five contracts, format and tidy pass in the corresponding
`bpr-0748-d25-correction-*` logs. The original green run is superseded for
acceptance by this corrected run; the red/green sequence is retained.
