---
schema: "implement-rfc.card.v1"
id: "BPR-2419"
title: "coding-tidy passes across the tree: every clang-tidy 22 finding is fixed without a change of behaviour, or kept with a scoped NOLINT that says why"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"chore","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"e4595ef66","reservation":"docs/issues/_reservations/BPR-2419--BPR-2419--90fbf931e68c.json","origin":"coordinator, 2026-11-26: a stable build; coding-tidy failed on many files predating recent work","acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2419: coding-tidy passes across the tree

## Why

`cmake --build --preset dev --target coding-tidy` (clang-tidy 22,
`--warnings-as-errors`) failed on roughly 150 translation units, most of
the findings older than the cards that touched those files: local names
in snake_case, `const` locals that blocked a move, an optional
dereferenced and wrapped again, identical switch branches, string
concatenation in loops, `std::endl` in courses, an accessor shadowing
`QWidget::actions()`, and a few analyzer paths. Cards could only claim
"tidy on touched units", and a touched unit kept failing on lines it did
not write.

## Change

Behaviour-preserving only; six commits, one per area.

1. **Core** (`src/core`, `tests/core`): locals renamed to camelBack;
   `const` dropped where it prevented a move (`returning_review.cpp`);
   `agent_cost.cpp` assigns the optional rather than its dereferenced
   value; the Windows CNG stub headers keep the SDK's names under
   `NOLINTBEGIN(readability-identifier-naming)`; `workflow_action.cpp`'s
   ordered gap chain keeps two `finding` branches under a scoped
   `bugprone-branch-clone` NOLINT.
2. **Qt adapter** (`src/adapters/qt`, `tests/adapters_qt`):
   `local_draft_workflow_area::actions()` is `action_rows()` (it shadowed
   `QWidget::actions()`; its one course updated); identical switch
   branches merged in `review_tour_editor.cpp` and
   `local_draft_workflow_area.cpp`; the dashed bar in
   `suggestion_editor.cpp` counts dashes in an integer; the copy in
   `agent_tab_view::press_offer` is kept with a NOLINT (the offer can live
   in `Offered`, which `ask()` rebuilds); `agent_report_view::render_dressed`
   and the home-drafts key filter are covered for the analyzer's
   construction-time null path.
3. **Storage and GitHub adapters**: the schema-widening statements in
   `agent_setup_store.cpp` are built with `append`; the test-only
   interruption step in `private_review_store.cpp` decrements outside its
   condition; a `from_chars` over a bounded range keeps a NOLINT.
4. **Application host**: renames, `emplace_back`, a declaration's
   parameter name matching its definition.
5. **Composition** (`src/app`): renames in `main.cpp` and
   `window_explain_file.cpp`; `profile_desktop.cpp` reserves its id list;
   NOLINTs with reasons in `review_agent_stack.cpp` (a testing build puts
   the probe in front of the launcher) and `release_update_desktop.cpp`
   (the analyzer's stack-address path through the dispatch's functions).
6. **Courses and fixtures**: renames; optional paths passed as optionals;
   `std::endl` written as `'\n' << std::flush` (same flush); check and
   fixture messages keep their concatenation under a NOLINT.

src/app files changed (the manifest pin needs a reseal):
`src/app/main.cpp`, `src/app/src/github_application/review_agent_stack.cpp`,
`src/app/src/github_application/window_explain_file.cpp`,
`src/app/src/profile_desktop.cpp`, `src/app/src/release_update_desktop.cpp`.
None of the concurrent agent-chat session code
(`window_agent_setup.cpp` `reset_conversation`, the invocation contracts)
was touched; the tidy pass found nothing in those files after the rebase.

## Acceptance

- AC1. `coding-tidy` (clang-tidy 22, warnings as errors) passes over
  every first-party translation unit in the dev compilation database.
- AC2. No behaviour changes: each fix is a rename, a `const` dropped, a
  redundant dereference removed, merged identical branches, an equivalent
  expression, or a scoped NOLINT naming its reason.
- AC3. The affected unit, Qt and course rows and the composed drawer
  course pass.
- AC4. `coding-format-check` passes.

## Evidence

Revision `e4595ef66` (on main `4de451619`), macOS arm64 Debug, dev preset.

- AC1: `CODING_STANDARD_TIDY_JOBS=8 cmake --build --preset dev --target coding-tidy`
  over 1469 translation units reported three findings, each fixed in this
  card; after the rebase onto `4de451619`, `tidy.py --warnings-as-errors`
  over those three units and every unit main changed since the run
  (`window_agent_chat.cpp`, `window_agent_setup.cpp`,
  `window_construction.cpp`, `fixture_effect_holds_and_agent_evidence.cpp`,
  `agent_chat_session_host.cpp`, `agent_session.cpp`,
  `agent_chat_reply_tests.cpp`, `agent_session_tests.cpp`,
  `agent_chat_composition_public_path.cpp`): "clang-tidy passed".
- AC2: review of the six commits; the dev build compiles every target.
- AC3: `ctest --test-dir build/dev -j 8` over 162 rows (every row whose
  executable compiles a changed test source, plus the rows of the changed
  core, Qt, storage, GitHub and host units): 161 passed, among them
  `qt_drawer_surfaces_fit_scale_100`, `_125`, `_200` and the composed
  drawer course `application_composed_drawer_fit_scale_100`, `_200` and
  `_cocoa`. The one failure, `application_thread_words_public_path`, is
  not this card's: its check refuses the item chat's "Carry this
  conversation over to the chosen agent" (`itemChatCarryOver`, stage 7 of
  RFC-BPR-0130, already on main), whose allow-list in
  `setup_destination_public_path_tests.cpp` names only `agentCarryOver`
  and `tourCarryOver`. No string, and no line of that check, changed here.
- AC4: `cmake --build --preset dev --target coding-format-check`:
  "Formatting check passed." (2504 files, clang-format 22.1.8).
