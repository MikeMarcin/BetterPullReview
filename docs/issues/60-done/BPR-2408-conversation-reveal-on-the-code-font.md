---
schema: "implement-rfc.card.v1"
id: "BPR-2408"
title: "The conversation-reveal course gives the comparison the application's code font, so the head pane's edit is checked row for row; BPR-2364's whole-history refusal still holds on main; two rows stale since BPR-2397 and BPR-2400 follow their sources"
status: "Done"
rfcs: []
depends_on: ["BPR-2375", "BPR-2374", "BPR-2403", "BPR-2397", "BPR-2400"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2408--BPR-2408--5ee6e9eb4fe6.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"4ce75886a","found_by":"BPR-2403's full dev suite run: qt_workbench_conversation_reveal fails every run on main; its implementer also saw the Seen and Approve again courses pass with the fixture's repository-size overlay switched off","related_cards":["BPR-2375","BPR-2364","BPR-2374","BPR-2403","BPR-2397","BPR-2400"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2408: The conversation-reveal course and BPR-2364's refusal on main

## Report

1. `qt_workbench_conversation_reveal` failed every run on main at
   `test_compose_in_the_drawer`: "the edit keeps the head pane's row pitch,
   and line 6, above the range, stays where it was". The head pane's edit
   (Suggest change on line 13) set its rows 14 px apart against the
   comparison's 15 px, and line 6 sat at 75 px against 80 px.
2. [[BPR-2403]]'s implementer saw `application_review_seen_public_path` and
   `application_home_approve_again_public_path` still pass with the
   fixture's repository-size overlay switched off, although [[BPR-2364]]
   refuses a whole-history fetch of unknown size.

## Cause

### 1. The course, not the product

Bisected over main's first-parent merges with the course instrumented to
print every row's top in the edit and in the head pane:

| Merge | Result | Head pane's top row |
|---|---|---|
| `5880ddbf7` ([[BPR-2374]]) | pass | line 6 (line 1 at −70 px) |
| `bc9d427ff` (BPR-2370) | pass | line 6 |
| `f5e2aea70` ([[BPR-2375]], `4ce2a2511`) | fail | line 1 (at 5 px) |
| `41b3b5373`, `287b746a5`, main | fail | line 1 |

[[BPR-2375]] changed how a draft opening in the drawer moves the code:
`whole_card_move` leaves a card already whole in the drawer's band where it
is, and moves the code only for a card that is cut. In this course the
draft for line 13 of a 30-line file is whole, so the comparison is no
longer scrolled when the edit opens. That is the intended behaviour.

The pitch mismatch was there before and after. The course's window had
no appearance profile, so the comparison's code font was Qt's default
"Sans Serif", which the offscreen platform sets in `.AppleSystemUIFont`, a
proportional face (`QFontInfo::fixedPitch` false). The edit is a
fixed-grid editor: `fixed_pitch_code_font` gives it Menlo instead, whose
row is 14 px against the proportional face's 15 px. The edit meets the
comparison at the pane's top visible row (`sync_head_edit`), so each row
further down drifts a pixel. With line 6 at the top (before BPR-2375) the
check passed by coincidence; with line 1 at the top, line 6 is five rows
down and five pixels off. The old check's 1 px tolerance on the pitch hid
the mismatch.

The application gives the comparison the profile's code font
(`window_workbench.cpp`, `apply_comparison_code_font_to`), which by default
is the bundled JetBrains Mono NL that `main` registers. With that font the
edit uses the same face, and every row above the range meets its own
(16 px pitch, 0 px drift). The product's diff panes are not misaligned.

### 2. BPR-2364's refusal is in force

`acquire_review_transport` (`src/application/src/epoch_host_support.cpp`)
is unchanged since [[BPR-2364]] (`606cf91a1`). No later commit bypasses
it. The fixture serves the repository's size twice:

- `879c6d2f3` "Report packed repository sizes in public-path fixtures"
  (2026-11-23, no card): `github_public_path_fixture.cpp`'s served-response
  path adds `size` to a `GetRepository` answer that has none, from the
  fixture Git transport's packs.
- [[BPR-2374]] (`fa87cd4d5`): `overlay_repository_size` adds `size` when the
  recording is loaded.

Switching off either one leaves the other, so the courses pass. Switching
off both, the courses stall exactly as [[BPR-2403]] recorded for a lane
without BPR-2374 ("Loading your pending comments… You can submit once they
have loaded."). The refusal applies; the fixture is covered twice. That is
correct behaviour and not a regression, so nothing in the product or the
fixture was changed here.

## Change

`tests/adapters_qt/workbench_window_tests.cpp`, `test_compose_in_the_drawer`:

- The window is given the application's appearance as the composition gives
  it: the bundled code font registered (`register_bundled_code_font`), the
  default profile applied to the window, its comparison code font applied to
  the comparison, and the comparison adopting the profile.
- The check is stricter: the edit's pitch equals the head pane's (under
  0.5 px), and every row of lines 1 to 12, above the range, meets its head
  row within 1 px. The failure names each row's top in the edit and in the
  pane.

`tests/workbench_case_inventory.json`: source SHA-256 resealed for the edited
test file (no case added or removed).

Two rows the full dev suite found stale, both broken after [[BPR-2403]]'s
run by changes that landed later the same day:

- `qt_composed_inline_drawer_scale_{100,125,200}`
  (`tests/adapters_qt/composed_github_tests.cpp`) failed at "Tab in the post
  reaches its link and says where it goes". Since [[BPR-2397]] (`21b7d9ecd`)
  the keyboard's link in a Markdown reader is a state drawn as a focus ring,
  not a selection, so the reader's caret is no longer on the link and its
  `anchorHref` is empty. The course now reads `keyboard_link()` ("the
  margins page") and checks that none of its words are selected; the
  Return that follows still opens the address question for
  `https://example.invalid/margins`.
- `application_thread_words_public_path`
  (`tests/end_to_end/setup_destination_public_path_tests.cpp`) failed at "no
  visible or accessible string calls a review thread a conversation" on the
  History menu's "Carry this conversation over to the chosen agent"
  (`agentCarryOver`, `tourCarryOver`, [[BPR-2400]] `69771bbd8`). That action
  moves an agent chat to another agent; it joins the other agent-chat
  wording the check sets aside.

## Acceptance

- AC1. `qt_workbench_conversation_reveal` passes on main.
- AC2. The stricter check fails when the comparison keeps a proportional
  code font.
- AC3. The cause of the BPR-2364 observation is shown, not inferred.
- AC4. The two stale rows pass and still fail on what they guard.
- AC5. The whole dev suite was run once; every failing row is listed with
  its cause.

## Evidence

Revision `4ce75886a`, macOS arm64 Debug, offscreen.

- AC1: `ctest --preset dev -R '^(qt_workbench_conversation_reveal|command_correctness_registry_contract|qualification_case_inventory_contract)$'`:
  3 of 3 passed, and the reveal row passed again in the full suite.
- AC2 (negative control): with the font registration skipped, the course
  fails with "the edit keeps the head pane's row pitch, and lines 1 to 12,
  above the range, stay where they were (edit/head: 5/5 19/20 33/35 47/50
  61/65 75/80 89/95 103/110 117/125 131/140 145/155 159/170)".
  Bisect: `bc9d427ff` passes, `f5e2aea70` fails (table above).
- AC3: `home_oversized_public_path` built with an environment switch on each
  overlay. Both on, `879c6d2f3`'s off, BPR-2374's off: both courses pass.
  Both off: both fail at the Approve wait, status "Loading your pending
  comments…". The experiment was reverted.
- AC4: after the change `qt_composed_inline_drawer_scale_{100,125,200}` and
  `application_thread_words_public_path` pass. Before it, the suite run
  failed them on the assertions quoted above (the negative controls).
- AC5: `ctest --preset dev -j 4 --output-on-failure` at the change minus
  the two stale-row fixes: 1103 rows, 1089 passed, 1 skipped, 13 failed.
  `qt_workbench_conversation_reveal` passed in it. The failures:
  - `qt_composed_inline_drawer_scale_{100,125,200}`,
    `application_thread_words_public_path`: stale, fixed here (AC4).
  - `application_large_review_responsiveness`, `_launch`, `_active_work`,
    `_pathological`, `_history`: Debug timing budgets
    (`LARGE_REVIEW_TIMING_LIMIT_MISSED`, retried once each), nongating by
    policy (AGENTS.md "Debug timings"). `_cache_bytes` skips.
  - `application_document_tabs_restoration_public_path`: its `auth-advance`
    child was killed (`-9`) right after the 538 s
    `application_profile_window_review_restoration_public_path`, while
    another checkout's suite ran on the machine. Rerun alone it passes in
    24 s. Load, not a defect found here.
  - `documentation_structure`, `documentation_issue_graph_contract`,
    `issue_id_allocation_contract`: the worktree lacks the MASK-deployed
    `.agents/skills/implement-rfc` (`No module named 'board_model'`). With it
    linked, the last two pass; `documentation_structure` and
    `verify_documentation.py` then report only the lane's
    `docs/issues/NEXT_ID` (BPR-2404) behind main's high water (BPR-2408),
    which clears when the lane takes main.
- Format: `_coding-standard/scripts/format.py` (clang-format 22.1.8) on the
  three touched C++ files leaves them unchanged.

The test executables of this tree were stripped of local symbols
(`strip -x`) to fit the disk.
