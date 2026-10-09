---
schema: "implement-rfc.card.v1"
id: "BPR-1362"
title: "The threads drawer no longer crashes on Windows when its forest is replaced"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win, the owed Windows run of BPR-1355's application_thread_words_public_path, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1362--BPR-1362--50aba54c9754.json","related_cards":["BPR-1355","BPR-0972"],"decision":"coordinator, 2026-10-09: land as proposed; key by value rather than rely on ordering"}
---

# BPR-1362: The threads drawer no longer crashes on Windows when its forest is replaced

## Observation

On Windows, `application_thread_words_public_path` ([[BPR-1355]]) crashed
with `0xC0000005`: once in four runs under ctest, five times in five run
directly (Debug, `7672d5a8b`). It died while waiting for "the Comments drawer
shows renderer.cpp's seven threads", right after the course selected a file in
Review with both drawers open. macOS and Linux pass.

The stack, captured with a temporary unhandled-exception filter (dbghelp):

```
std::_Hash<... basic_string_view ... >::emplace          (write at 0x000000000000000A)
bpr::qt::conversation_cards_view::index_retained          conversation_cards_view.cpp:573
bpr::qt::conversation_cards_view::adopt                   conversation_cards_view.cpp:513
bpr::qt::github_conversation_panel::refresh_projection    github_conversation_panel.cpp:756
bpr::qt::github_conversation_panel::set_current_endpoint_placement   :998
bpr::qt::workbench_window::set_conversation_endpoint_placement       workbench_window.cpp:12666
bpr::app::github_application::implementation::request_diff           github_application.cpp:15238
...
```

The Debug CRT heap check (every 16 allocations) found no corrupt heap block;
the damage was inside the map's own state.

## Cause

`conversation_cards_view::RetainedById` was an
`std::unordered_map<std::string_view, std::size_t>` whose keys viewed
`Retained[i].Id` ([[BPR-0972]]).
- `adopt` reassigned `Retained` (`Retained = aConversations`), and `clear()`
  emptied it.
- Only afterwards did `index_retained()` call `RetainedById.clear()`, when
  every stored key was a view into a replaced or freed string.

MSVC's `unordered_map::clear()` (`VC/include/xhash`) erases element by element
when the hash is `noexcept` and there are more than eight buckets per element.
To do so it re-hashes each stored key. A large forest followed by a smaller one
takes that path. Hashing freed strings reset the wrong buckets and left stale
bucket pointers, and the next `emplace` wrote through one. libstdc++ and
libc++ never hash in `clear()`, so the bug was invisible elsewhere.

## Outcome

`RetainedById` keys by value (`std::string`), so no key can outlive the text it
names, whatever the order of assignment and re-indexing. Every lookup already
passed a `std::string`. The cost is one small string copy per node per adopted
forest.

The other `string_view`-keyed hash containers in `src/` were checked. All are
function locals over data that outlives them, so none has this hazard.

## Acceptance

- `qt_conversation_cards_view`'s
  `test_replacing_a_forest_never_reads_the_last_one` does the following, then
  clears and adopts again:
  - adopts 3,000 threads, so the map has thousands of buckets;
  - then 250 threads and then 240, each with other long identities, so few keys
    sit in many buckets and MSVC's per-element clear() runs.
  Against the old code it crashes on Windows Debug (`0xC0000005` in 0.65 s).
  A first version with forests of 2 and 3 threads passed: so few stale buckets
  were rarely hit. With the fix it passes. On other compilers it passes either
  way.
- On Windows, `application_thread_words_public_path` passes repeatedly, with
  `qt_conversation_cards_view` and the conversation panel rows.

## Windows results (bpr-main-win, 2026-10-09)

On `e6e3b8f67` with this change, Windows 11, MSVC 19.50,
`build/windows-dev-tour-cli` (Debug), offscreen:

- **Control:** `qt_conversation_cards_view` with the new test against the old
  `string_view` key crashes (`0xC0000005`, 0.65 s).
- **Fix, 10 of 10 pass:** `qt_conversation_cards_view`,
  `qt_github_conversation_panel`, `qt_github_conversation_model_property`,
  `qt_conversation_finder_scale_{100,125,200}`,
  `qt_public_preview_conversation`, `qt_workbench_conversation_reveal`,
  `application_setup_destination_public_path` and `architecture_boundaries`.
- **`application_thread_words_public_path`:** 5 of 5 pass
  (`--repeat until-fail:5`). Before the fix it crashed in 3 of 4 runs alone and
  5 of 5 run directly.
