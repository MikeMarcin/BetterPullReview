---
schema: "implement-rfc.card.v1"
id: "BPR-1361"
title: "Post observations refuse a temporary source at compile time"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win, BPR-1092's Windows run, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1361--BPR-1361--1006a85e53eb.json","related_cards":["BPR-1349","BPR-1087"],"filed_by":"bpr-main-win, at the coordinator's request"}
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
