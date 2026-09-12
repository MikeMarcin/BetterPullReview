---
schema: "implement-rfc.card.v1"
id: "BPR-0744"
title: "Preprocessor directives are grey but their arguments take their own roles"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decisions":["fed7a1bf-e20c-4731-ad72-387b31d29b98","4effa6f2-84e6-459c-8491-0e7e7c2ab3ca"]}
acceptance: [{"criterion":"syntax_classification_contract pins over tests/fixtures/syntax/identifier_roles.cpp that in #ifndef MACRO and #define MACRO the directive keyword is syntax_preprocessor and the name is syntax_macro at its own exact range, and that an #include path is the string role at its own range while the #include keyword stays grey; the old assertions are replaced, not added beside","outcome":"pass","revision":"c0d3c5da","evidence":{"kind":"command","detail":"Unchanged and reused from c0d3c5da: nothing in src/adapters/syntax or tests/syntax_contract has moved since, and the v009 bundle is built from that classifier. The run and its falsifiability check are recorded in the card's Slice and Verification sections."}},{"criterion":"test_the_dark_palette_follows_the_visual_assist_scheme is updated for the value the include path takes, and core_appearance_contrast and the headroom ratchet pass; the contrast of the chosen path colour on the added and removed row fills through the selection wash is measured before it is pinned","outcome":"pass","revision":"c0d3c5da","evidence":{"kind":"command","detail":"Unchanged and reused from c0d3c5da; no palette value has moved since and appearance_profile.cpp is untouched by d22."}},{"criterion":"qt_appearance_*, qt_syntax_pane_composition, sqlite_appearance_settings_storage, application_appearance_*, application_composition_contract and architecture_boundaries pass","outcome":"pass","revision":"f953ce9c","evidence":{"kind":"command","detail":"11 of 11 at f953ce9c, every one rerun at this revision rather than reused. The profile freed at 2026-09-12T17:45:33Z when the owner quit his v009 probe instance ('I quit the running instance'); verified before running rather than taken on trust -- pid 72017 gone, no BetterPullReview process at all, nothing holding epoch-catalog.sqlite3 or activate.sock, and op=application outcome=shutdown logged at that instant. ctest --preset dev --output-on-failure -R '^application_composition_contract$' is Passed in 172.22s, exit 0 (build/logs/bpr-0744-d23-composition.log) -- this is the subtest test_tests_off_release_graph_omits_public_path_fixture that d20 and d22 could not take, and it went green on the first attempt with no change to it, which confirms d22's account that the block was the busy profile lease and nothing else. The remaining ten are 10 of 10 in 124.78s, exit 0 (build/logs/bpr-0744-d23-ac3-rest.log): architecture_boundaries, application_appearance_host, application_appearance_profile_consumers, sqlite_appearance_settings_storage, qt_appearance_settings_panel, qt_syntax_pane_composition, qt_appearance_profile, application_appearance_theme_customization_contract, application_appearance_gesture_contract and application_appearance_store_fallback_contract. They were not reused from bbc4db98 even though they could have been: git diff --stat bbc4db98 HEAD -- src tests packaging is empty, so the inputs are unchanged, but the whole set is taken at one revision so the criterion rests on a single run."}},{"criterion":"The owner accepts a C++ header on the dark palette in an attended session: the criterion BPR-0720 and BPR-0738 both still carry, asked once more on a bundle with these ranges","outcome":"pass","revision":"aff9fc64","evidence":{"kind":"artifact","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0744-directive-ranges, revision app-release-c2381f32-2026-09-12.5 (bundle built at aff9fc64, executable sha256 c2381f325093200563544ce3bb9e1ba04f847102e2f5b599ca6cc51c6f555653 recomputed on disk and equal), fingerprint a321ed197669c28e51b7945dd1352a9af634ae12cf63b63b71e1338513692eec equal to the prepared candidate's in session-identity.json, decision 4f1d1354-c3c1-43d9-af11-358347147453, actor human, 2026-09-12T16:06:15Z, sequence 360: accept, note EMPTY. Judged against the running bundle, not from memory: the v009 image ran 16:03:30Z to 16:06:34Z per op=application in the profile's diagnostics.log and the decision was saved at 16:06:15Z. The one new question that rode this candidate -- whether every identifier inside #if, #ifdef, #ifndef, #elif, #undef and defined() should take the macro purple rather than only ALL_CAPS ones -- is NOT answered by an accept of the implemented behaviour, is not read into the empty note, and is not implemented. It is carried with its costing to BPR-0748."}}]
---

# Preprocessor directives are grey but their arguments take their own roles

The owner, 2026-09-11, in the v008 attended session, on two candidates.
Rejecting `BPR-0720-dark-palette` (decision `fed7a1bf`, 04:28:03Z), verbatim:

> comments are green, things are mostly right, but preprocess being grey is
> taken too far compared to the reference. The preprocessor directives
> themselves should be grey but not necessarily their arguments. Macros in
> preprocessor should still color as macros (purple). Strings in #include
> statements should color as the reference image

Rejecting `BPR-0738-identifier-colours` (decision `4effa6f2`, 04:36:00Z),
verbatim:

> As I answered in another question, most things are good now but include
> paths should be painted like the reference and things like macros in
> #ifndef MACRO should be painted as macros.

The screenshot is
[stdexec's `__detail` header in Read](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-dark-palette-preprocessor-2026-09-11.png):
`#include "../../stdexec/__detail/__config.hpp"`, `#if STDEXEC_USE_MODULES()`,
`#else`, `# include <concepts>` and `#endif` are grey end to end. The
reference is
[Visual Studio 2022 dark with Visual Assist](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-visual-studio-reference-2026-09-11.png),
where `#ifndef`, `#define`, `#include` and `#pragma` are grey, the macro name
after `#ifndef` and `#define` is purple, and the include path is
string-salmon.

This settles the first of the two open questions BPR-0738 carried: the
include path is painted like the reference, not grey. It also revises the
2026-09-11 owner direction recorded on RFC-BPR-0006, whose parenthetical
reads "grey preprocessor lines including the include path"; that stays true
of the directive and no longer of its arguments.

## What the code says

The classifier can already emit different roles for different ranges inside
one line: `span_collector::applyFormat` is called once per format range
KSyntaxHighlighting reports, at arbitrary sub-line offsets, and appends a
span per range
(`src/adapters/syntax/src/packaged_syntax_classifier.cpp:426-454`); adjacent
equal-role ranges are merged afterwards (`:497-506`). Two things collapse a
directive line anyway:

- The packaged C++ grammar carries `attribute="Preprocessor"` as the default
  attribute of its preprocessor contexts, so `#ifndef MACRO` and
  `#if STDEXEC_USE_MODULES()` arrive as a single Preprocessor range
  (`build/.cpm-cache/bprksyntaxhighlighting/8f28/data/syntax/isocpp.xml:748-750`,
  `:753-757`, `:770-775`, and `:779-786` where `Define` gives the macro name
  `attribute="Preprocessor"` at `:785`).
- Our own identifier post-pass runs only over `syntax_normal` runs
  (`packaged_syntax_classifier.cpp:435-448`): a Preprocessor run is appended
  and returned before `classify_identifiers` (`:458-484`) and
  `identifier_role` (`:365-366`, `is_macro_case` at `:240-255`) can look
  inside it. That single gate is why `#ifndef MACRO` never reaches the
  ALL_CAPS rule that already exists.

The include path is a separate question. The grammar gives `<…>` and `"…"`
inside an `#include` the `Prep. Lib` attribute, which maps to `dsImport`
(`isocpp.xml:741-746`, `:910`), and the classifier maps `Import` to
`core::appearance_role::syntax_import`
(`packaged_syntax_classifier.cpp:76-77`), painted `A3A3A3` on dark
(`src/adapters/qt/src/appearance_profile.cpp:180`). Repainting
`syntax_import` itself would repaint `import`, `from` and `@import` in
roughly fifty-five other packaged grammars (python, java, javascript,
typescript, kotlin, swift, css/less/scss, cmake, makefile, objectivec and
the rest), which is not what the owner asked for. Mapping `Import` to
`syntax_string` only where the identifier heuristics already gate on C and
C++ (`:124-128`) has no collateral outside those definitions and is the
narrower change.

## Scope

- A preprocessor directive keyword (`#include`, `#define`, `#ifndef`,
  `#ifdef`, `#if`, `#else`, `#endif`, `#pragma`) stays `syntax_preprocessor`
  grey. Its arguments do not inherit that role: an ALL_CAPS word in a
  directive takes `syntax_macro`, the role BPR-0738 added, and paints the
  same purple a macro paints in ordinary code.
- An `#include` path, in angle brackets or quotes, paints as the reference
  paints it. Prefer the C/C++-scoped classifier mapping to `syntax_string`
  over repainting `syntax_import`; if the slice repaints a role instead, it
  says on this card which and why, and what it does to the other fifty-five
  grammars.
- Decide, and record, what happens to the rest of a directive's text that is
  neither the keyword nor a macro nor a path: `once` after `#pragma`, the
  operators in `#if A && B`, a numeric literal. Grey is an acceptable answer;
  an unexamined one is not.
- Prefer the post-pass over forking the grammar. Editing the packaged
  definition moves `BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256` and the offline
  cache contract (`cmake/BprSyntaxHighlightingProvider.cmake:230,262-266`)
  and is out of proportion to the change.
- The light and high-contrast profiles are untouched, and the diff colour
  themes stay off-limits.
- Owned paths: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`,
  `tests/syntax_contract/syntax_classification_contract_tests.cpp` with
  `tests/fixtures/syntax/identifier_roles.cpp`, and
  `src/adapters/qt/src/appearance_profile.cpp` with
  `tests/adapters_qt/appearance_profile_tests.cpp` only if a dark value
  moves.

## Blast radius

No appearance store schema move: schema 5
(`src/adapters/sqlite/src/appearance_settings_database.hpp:29`) exists
because BPR-0738 added a role
(`appearance_settings_database.cpp:306-316`); a role-to-range change adds no
role and changes no column, so `palette_stored` is untouched and there is no
second reset for the owner. No customisation panel change: it iterates
`appearance_role_count` and labels from `stable_name`. No composition digest
repin unless `src/app/src/github_application.cpp` is edited, which this slice
does not.

## Acceptance

- `syntax_classification_contract` pins, over
  `tests/fixtures/syntax/identifier_roles.cpp`, that in `#ifndef MACRO` and
  `#define MACRO` the directive keyword is `syntax_preprocessor` and the name
  is `syntax_macro` at its own exact range, and that an `#include` path is
  the string role at its own range while the `#include` keyword stays grey.
  The existing assertions at
  `tests/syntax_contract/syntax_classification_contract_tests.cpp:664-669`
  ("a directive is not a macro", one span over `#define
  BEGIN_TEST_METHOD_PROPERTIES`) are the ones the owner's direction reverses;
  replace them rather than adding beside them.
- `test_the_dark_palette_follows_the_visual_assist_scheme`
  (`tests/adapters_qt/appearance_profile_tests.cpp:295-330`) is updated for
  whatever value the include path takes, and `core_appearance_contrast` and
  the headroom ratchet (`appearance_profile_tests.cpp:2158-2210`, dark at one
  hundredth at `:2176`; the 4.5:1 floor at `:71` and `:366-372`) pass. The
  contrast of the chosen path colour on the added and removed row fills,
  through the selection wash, is measured before it is pinned.
- `qt_appearance_*`, `qt_syntax_pane_composition`,
  `sqlite_appearance_settings_storage`, `application_appearance_*`,
  `application_composition_contract` and `architecture_boundaries` pass.
- The owner accepts a C++ header on the dark palette in an attended session:
  this is the criterion BPR-0720 and BPR-0738 both still carry, asked once
  more on a bundle with these ranges.

## Slice, 2026-09-12 (d20)

Three changes, all in the C and C++ post-pass behind the existing
`uses_identifier_heuristics()` gate (`packaged_syntax_classifier.cpp:124-137`,
`aName == "C++" || aName == "C"`). The packaged grammar is untouched, no
appearance role was added, no palette value moved, and
`src/app/src/github_application.cpp` was not edited, so there is no schema
move, no second palette reset and no digest repin.

- **A directive run is split instead of appended whole.** `classify_directive`
  walks one `syntax_preprocessor` run and emits grey, macro, grey at exact
  ranges: an ALL_CAPS word anywhere in the run takes `syntax_macro`, which is
  what `#ifndef MACRO`, `#define MACRO` and `#if MACRO()` needed.
- **`Import` becomes `syntax_string`, C and C++ only.** `role_of` still maps
  `text_style::Import` to `syntax_import` for every grammar; the substitution
  is in `c_family_role` under the gate. `syntax_import` keeps all four of its
  values and still names `import`, `from` and `@import` in the other ~55
  packaged grammars. The include path therefore takes `syntax_string`,
  `D69D85` on dark — an existing already-pinned value, so its contrast on the
  added and removed row fills through the selection wash is already measured by
  `core_appearance_contrast` and the headroom ratchet. Nothing new to measure
  because no new value exists.
- **`syntax_other` becomes `syntax_macro` when the run is ALL_CAPS**, same
  place, same gate. The grammar styles a predefined macro it happens to list as
  `Standard Macros` → `dsOthers` (`isocpp.xml:911`), painted `B1BAC4`, a
  near-grey. That left `__GNUC__` near-grey on the same line where `__APPLE__`
  was already purple, with upstream's keyword list as the only reason. It was
  the card's own requirement unmet, not new scope.
- `identifier_heuristic_revision` is 1 → 2. It is folded into the C and C++
  definitions' published version, so every span set cached under the old role
  ranges is keyed away rather than served stale.
- One comment in `appearance_profile.cpp` was corrected: the dark block read
  "grey preprocessor lines including the include path", the exact sentence this
  card reverses. No value near it changed.

### The ALL_CAPS gate on `syntax_other` is load-bearing

A blanket `syntax_other` → `syntax_macro` under the C/C++ gate is wrong, and
the check that says otherwise is the wrong check. `dsOthers` has exactly one
user in `isocpp.xml` and none in `c.xml`, but what reaches `textStyle()` is the
format stream, and `isocpp.xml` pulls rules from three other grammars whose
`dsOthers` itemData enter it identically: `gcc.xml:600` (`GNU Macros`, included
at `isocpp.xml:767,778,785`), `doxygen.xml:606,610` (`Entities`, `Identifier`,
via `##Doxygen` at `:340`) and `modelines.xml:172-174` (`Value`,
`Option ON/OFF`, via `##Comments` at `:702,710,825,869`). Probed rather than
reasoned: a blanket substitution paints `&amp;` inside a Doxygen comment and
`on`/`off` inside a kate modeline the macro purple, which breaks the invariant
this suite pins twice — no heuristic role inside a line or block comment.

`is_macro_case` separates the two populations exactly, because it requires at
least one capital and rejects any unit that is not a capital, digit or
underscore. `__GNUC__`, `__ELF__`, `__SIZEOF_INT128__`, `__FILE__` and
`__VA_ARGS__` pass; `&amp;`, `on`, `off`, `1`, `0`, Doxygen's HTML attribute
names and the modeline keyword lists (`on|true|1`, `off|false|0`, all
lowercase, `casesensitive="1"`) cannot. **Do not remove that condition.**

Residual, named rather than hidden: a kate modeline with an ALL_CAPS value
(`// kate: encoding UTF8;`) would take the purple inside a comment. `UTF-8` and
`C++` cannot, since `-` and `+` fail the test. Judged acceptable against the
cost of teaching the classifier which runs are inside a comment, which it does
not currently know.

### The rest of a directive's text: decided, grey

Everything in a preprocessor run that is not an ALL_CAPS word stays
`syntax_preprocessor`: the keyword with or without `#  ` indentation, `once`,
`defined`, `&&`/`!`/`>=`/`##`, parentheses, single-letter macro parameters,
lowercase identifiers, and any double-quoted region. The quoted-region rule is
a deliberate refinement beyond the card's literal wording: without it
`#error "SOME MESSAGE about CONFIG"` painted three prose words purple. Parts
the grammar already styles for itself arrive as their own runs and never reach
the new pass, and keep their roles: `()` after a function-like macro name
(`syntax_operator`), `1` in `#define X 1` (`syntax_number`), a trailing comment
(`syntax_comment`), an `#if 0` block (`syntax_comment`).

Accepted consequence: `GCC` in `#pragma GCC diagnostic` and `STDC` in
`#pragma STDC` take the macro purple, because they are ALL_CAPS tokens under
the rule the card states.

### Two limits, pinned as facts rather than left to drift

A predefined macro spelled with no capital letter stays near-grey. Two disjoint
causes, both visible on the lines the owner is reading:

- listed upstream but lowercase → `syntax_other` `B1BAC4`: `__cplusplus`,
  `__linux__`, `__linux`.
- not listed upstream and lowercase → `syntax_preprocessor` `A3A3A3`:
  `__clang__`, `__xlC__`, `__xlc__`.

`is_macro_case` was **not** widened: accepting all-lowercase-with-underscores
would sweep up `text`, `aValue` and every ordinary snake_case identifier in a
normal run. Instead the contract pins all three colours on one fixture line,
`#if defined(__GNUC__) && !defined(__clang__) && __cplusplus >= 202002L`, so
neither limit can drift unnoticed or be mistaken for an accident.

**The rule that would remove both limits, costed, for the owner to choose.**
[cpp.cond]/4 makes it exact: after replacement, every remaining identifier in
an `#if` or `#elif` controlling expression is replaced by `0`, so it either was
a macro or is being tested as one; in `#ifdef`, `#ifndef` and `#undef` the
single identifier is a macro by definition. The exceptions that must stay grey
are `defined`, `true`, `false` and the `__has_include` / `__has_cpp_attribute`
/ `__has_builtin` operator family, and `#define` must stay excluded because its
replacement tokens are not macros. About 35 lines. The work is not the rule but
the state: a directive's run is split (`#if defined(` … `__APPLE__` …
`) && (defined(`), so a per-run keyword lookup sees only the first run; it
needs one per-line member recording which keyword opened the line, set on the
line's first preprocessor run and cleared in `begin_line`, which already resets
per line. Plus about four fixture lines, five assertions and a revision bump.
It is confined to directive lines and leaves ordinary code untouched. It is
asked in the v009 packet rather than taken: it puts more colour on a directive
line than the owner's reference screenshot shows, and while his complaint was
about too little colour, that is still a visible change to a shape he has not
seen.

### Verification

- `cmake --build --preset dev` clean, no `error:` or `warning:`.
- `ctest --preset dev` over `syntax_classification_contract`, `qt_appearance_*`,
  `core_appearance_*`, `qt_syntax_pane_composition`: 6 of 6. The wider run
  before the `syntax_other` change covered `sqlite_appearance_settings_storage`,
  `application_appearance_*`, `architecture_boundaries` and
  `qt_diff_workbench_scale_{100,125,200}`: 16 of 17, the one failure being the
  environment-blocked contract below. `application_review_round_trip_composition_contract`
  (the digest contract) passes; nothing was repinned.
- `format_check.py` and `tidy.py --warnings-as-errors` clean on the touched
  translation units.
- **The contract is falsifiable, checked rather than asserted.** HEAD content
  was restored into the classifier and the new contract run against it: it
  fails with "an angle-bracketed include path is the string role at its own
  range", and the macro half is equally red, one span of the directive role
  over `#ifndef BPR_FIXTURE_GUARD` with no macro span inside it.
- **Direct evidence over a real header, not only a fixture.** A probe outside
  the repository linked `libbetter_pull_review_adapter_syntax.a` and ran
  `packaged_syntax_classifier::classify()` over Qt's own
  `qsystemdetection.h` after `definition_for_path()` selected C++. Span total
  identical at 865 before and after. Role counts moved in exactly the expected
  places: `syntax_macro` 62 → 196, `syntax_import` 3 → 0, `syntax_string`
  0 → 3, `syntax_preprocessor` 344 → 391, and every other role byte-identical.
  The `syntax_other` step alone moved exactly four spans, all of them
  `__GNUC__`, `__ELF__` and `__SIZEOF_INT128__` twice; the two survivors are
  `__linux__` and `__linux`, the recorded limit.
- Span counts rise about 26% on directive-dense headers (688 → 865 on that
  header). `MaximumSpans` is 1,000,000 at every call site, so there is no new
  `limit_exceeded` risk, but it is a real increase in cache size.

### Red at HEAD, not this slice

`application_composition_contract`, subtest
`test_tests_off_release_graph_omits_public_path_fixture`, fails
`AssertionError: 1 != 0 : PROFILE_ALREADY_OPEN`. Verified at HEAD by restoring
`git show HEAD:` content over all six files: identical failure, identical
subtest. `assert_release_self_test`
(`tests/end_to_end/composition_contract_tests.py:508`) launches the built
executable with `os.environ.copy()`, so it opens the real user profile, and
`src/app/main.cpp:8019` prints that string for
`github_application_startup_failure::owner_busy`. The owner's v008 release
bundle has been running since 21:18 (pid 30378) and holds the profile. It was
not killed: it is the owner's session and the bundle under review. The check
goes green when he quits it, which the v009 packet asks for first. Not a digest
failure — the digest subtests pass in both runs.

### Carded, not fixed

[[BPR-0747]]: a `//` comment on a line whose only content before it is `#` and
whitespace paints `syntax_error` salmon. Upstream grammar behaviour, four
occurrences in `qsystemdetection.h`, identical count at HEAD.

## Attended verdict, 2026-09-12 (v009): accepted

The owner accepted `BPR-0744-directive-ranges` (revision
`app-release-c2381f32-2026-09-12.5`, fingerprint `a321ed19…` equal to the
prepared candidate's in
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json),
decision `4f1d1354-c3c1-43d9-af11-358347147453`, actor human,
2026-09-12T16:06:15Z), **note empty**.

He judged it with the bundle open, not from memory: the v009 image ran from
16:03:30Z to 16:06:34Z and he saved this decision at 16:06:15Z
(`~/Library/Application Support/BetterPullReview/BetterPullReview/`
`logs/diagnostics.log`, `op=application`). AC4 is satisfied, and with it the one
criterion [[BPR-0720]] and [[BPR-0738]] have both carried since v005: a
directive keyword is grey, an ALL_CAPS argument takes the macro purple, a
predefined macro like `__GNUC__` takes it too, and an `#include` path takes the
string salmon.

**The question that rode the candidate is not answered.** Whether every
identifier inside `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()`
should take the macro purple rather than only the ALL_CAPS ones was costed in
"Two limits, pinned as facts rather than left to drift" above and deliberately
not taken. An accept of what was implemented is not an answer about what was
not. It is **not** read into the empty note and it is **not** implemented on the
strength of the accept; it is carried to [[BPR-0748]] with its costing, so it
survives this card closing. The two limits stay pinned facts until he answers.

That it came back unanswered is the second time a question riding another
criterion has come back unanswered, after v008's "As I answered in another
question". BPR-0748 records that the next packet must ask it as its own
candidate with its own criterion, and with two pictures rather than a sentence.

**AC3 is still the only thing between this card and Done.**
`application_composition_contract`'s
`test_tests_off_release_graph_omits_public_path_fixture` remains unrun at this
revision, for the same reason it was unrun at `c0d3c5da` and worse: its
`assert_release_self_test` launches a `BUILD_TESTING=OFF` Release executable
with `os.environ.copy()`, and that build has no `--test-application-data-root`
at all — the flag is inside `#if defined(BPR_BUILD_TESTING)`
(`src/app/main.cpp:7261`, `:7285-7291`), so there is no way to point it at an
isolated profile. The owner's own probe run, pid 72017, has held the profile
since 16:06:37Z and was still holding it when this was written. Killing it to
take the measurement was refused: it is his process on his desktop. The check is
one command once he quits it, and it is the exact remaining ask carried in the
d22 frontier.

## AC3 closes, 2026-09-12 (d23): the profile freed and the contract went green

The one criterion this card carried unsatisfied through d20 and d22 was never
about the classifier. It was `application_composition_contract`, whose subtest
`test_tests_off_release_graph_omits_public_path_fixture` runs the freshly
built `BUILD_TESTING=OFF` Release executable against the real environment and
so cannot pass while anything holds the profile lease.

The owner quit his v009 probe instance at **2026-09-12T17:45:33Z** ("I quit
the running instance", logged as `op=application outcome=shutdown`). That was
verified before anything was run, not taken on trust: pid 72017 is gone, there
is no BetterPullReview process at all, and nothing holds
`state/v1/epoch-catalog.sqlite3` or `activate.sock`.

`ctest --preset dev --output-on-failure -R '^application_composition_contract$'`
then **passed in 172.22 seconds on the first attempt**, with no change made to
the test, the executable or the composition. That is worth recording as a
result and not only as a green: it confirms d22's diagnosis that the block was
the busy lease and nothing else, which had been established from the code
(`tests/end_to_end/composition_contract_tests.py:500-531` running with
`os.environ.copy()`, and `src/app/main.cpp:8005-8021` printing only
`PROFILE_ACTIVATION_QUEUED` or `PROFILE_ALREADY_OPEN` with the lease taken)
but never demonstrated. It was right, and neither the `HOME` relocation nor
killing his process — both refused at the time — would have shown anything
this run did not.

The other ten tests in the criterion were **rerun at this revision rather than
reused**, although reuse would have been defensible:
`git diff --stat bbc4db98 HEAD -- src tests packaging` is empty. The whole
criterion now rests on one revision's runs. 11 of 11, exit 0
(`build/logs/bpr-0744-d23-composition.log`, `build/logs/bpr-0744-d23-ac3-rest.log`).

The `#if`/`#ifdef`/`#elif`/`#undef`/`defined()` broadening question is **not**
closed by this and is **not** implemented on the strength of the accept. It
stays on [[BPR-0748]] with its costing, to be asked as its own candidate with
its own criterion and two pictures.
