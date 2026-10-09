---
schema: "implement-rfc.card.v1"
id: "BPR-1361"
title: "Post observations refuse a temporary source at compile time"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win, BPR-1092's Windows run, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1361--BPR-1361--1006a85e53eb.json","related_cards":["BPR-1349","BPR-1087"],"filed_by":"bpr-main-win, at the coordinator's request"}
acceptance: [{"criterion":"A compile-fail check proves feedback_post_observations(observe_feedback_sources(s)) is rejected","outcome":"pass","revision":"766fec883","evidence":{"kind":"command","command":"ctest --test-dir build/windows-dev-tour-cli -C Debug -R \"^application_feedback_counters$\"","detail":"static_asserts over requires-checks in feedback_counters_tests.cpp: feedback_post_observations, counted_posts and core::labels_of accept const T& and T& and refuse T and const T (deleted const&& overloads). The repository had no compile-negative pattern; a standalone MSVC control (cl /std:c++latest) showed the same check accepts a temporary when the deleted overload is absent."}},{"criterion":"The product callers and application_feedback_counters, application_feedback_counter_cost, application_overview_feedback and the preflight rows build and pass","outcome":"pass","revision":"766fec883","evidence":{"kind":"command","command":"ctest --test-dir build/windows-dev-tour-cli -C Debug -j4 -R \"^(core_feedback_.*|core_review_preflight|application_feedback_counters|application_feedback_counter_costs|application_overview_feedback|qt_feedback_label_controls|tour_cli_labels|architecture_boundaries)$\"","detail":"Windows 11, MSVC 19.50, full Debug build of the tour-cli tree passes (every caller of both builders compiles); 17 of 17 rows pass."}},{"criterion":"The card lists the other view-returning builders checked, with what was done","outcome":"pass","revision":"766fec883","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1361-post-observations-refuse-a-temporary-source.md","detail":"labels_of gained the same deleted overload; enum names, catalogs and static strings cannot dangle; header-edit and markdown requests are consumed within one call; feedback_text already takes a borrowed view; member accessors are outside this card."}}]
---

# BPR-1361: Post observations refuse a temporary source at compile time

## Observation

`application::feedback_post_observations(const feedback_sources&)` returns
`core::feedback_post_observation`s. Each of these is a set of
`std::string_view`s into the sources, and its declaration says only, in a
comment, that the sources "must outlive the result"
(`src/application/include/bpr/application/feedback_sources.hpp`).

The compiler accepts a temporary. [[BPR-1349]] Stage 2's seeded-rounds test
did exactly that: `counted_posts(observe_feedback_sources(snapshot))`. Its
posts dangled. On Windows Debug the freed strings were overwritten, the
refresh was refused as `duplicate_post`, and the test aborted on an empty
optional (`0xC0000409`). Linux and macOS passed by undefined behaviour. Fixed
in `7672d5a8b`.

The product's callers are correct today. Each keeps a named `sources`:
`src/application/src/feedback_counters.cpp` (`auto sources =
observe_feedback_sources(aSnapshot)`) and
`src/application/src/review_preflight_observation.cpp`.

## Outcome

Passing a temporary `feedback_sources` where the result views it does not
compile:
- `feedback_post_observations` gains a deleted rvalue overload
  (`auto feedback_post_observations(feedback_sources&&) = delete;`);
- so does any helper that forwards the sources and returns the views, such as
  `counted_posts` in `tests/application/feedback_counters_tests.cpp`.

The lvalue overload and its behaviour are unchanged.

A pass over the other functions that return views into an argument checks
whether the same hazard exists there, for example other
`*_observations(const …&)` builders feeding `core::` refreshes. Each one found
gets the same deleted overload, or is listed here with the reason it can't
dangle.

## Acceptance

- A compile-fail check proves `feedback_post_observations(observe_feedback_sources(s))`
  is rejected: a `try_compile` row, or the repository's existing
  compile-negative pattern if there is one.
- The product callers and `application_feedback_counters`,
  `application_feedback_counter_cost`, `application_overview_feedback` and the
  preflight rows build and pass.
- The card lists the other view-returning builders checked, with what was done.

## Done (bpr-main-win, 2026-10-09)

**Refused at compile time.** Each of the following has a deleted
`const T&&` overload, so a prvalue or `const` rvalue argument selects it and
does not compile:
- `application::feedback_post_observations`;
- `core::labels_of(const feedback_label_columns&)`, which returns a span into
  its argument and had the same hazard;
- the test helper `counted_posts`.

One core test passed a temporary to `labels_of` within a single expression,
which was safe. It now names the value.

**Proof** (`tests/application/feedback_counters_tests.cpp`): concepts over a
`requires` call with `static_assert`s.
- Each builder accepts `const T&` and `T&`, and refuses `T` and `const T`.
- A standalone MSVC control compiled the same check over a function without
  the deleted overload: a temporary was accepted. So the assertion fails
  exactly when the guard is missing.

**Other view-returning builders checked** (every free function in
`src/core/include` and `src/application/include` that returns a
`std::string_view`, `std::span` or view-holding type):
- **Static storage, cannot dangle:** `stable_name`/`name` of an enum,
  `feedback_label_catalog`, `definition_of`, `emphasis_pairs` and
  `agent_search_path_variable`.
- **Request types consumed within one call:** `feedback_header_edit_request`,
  `markdown_link_observation` and `pull_request_page_observation`.
- **`feedback_text(std::string_view, feedback_span)`:** takes a borrowed view
  already. Refusing `std::string&&` would also refuse valid same-expression
  uses, so it is left as is.
- **Member accessors** (`records()`, `rules()`, `root_identity()` and the
  like): the general lifetime of `*this`, outside this card.

**Windows results** (Debug, tour-cli tree, this change on `6eb957f6c`):
- The full build passes, with every caller of both builders.
- 17 of 17 pass: `core_feedback_*` (11 rows), `core_review_preflight`,
  `application_feedback_counters`, `application_feedback_counter_costs`,
  `application_overview_feedback`, `qt_feedback_label_controls`,
  `tour_cli_labels` and `architecture_boundaries`.
