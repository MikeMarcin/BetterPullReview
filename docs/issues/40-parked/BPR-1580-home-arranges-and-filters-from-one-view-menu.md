---
schema: "implement-rfc.card.v1"
id: "BPR-1580"
title: "Home arranges and filters from one View menu whose groups are labelled"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"I wonder if we should collapse these 3 into one. I don't understand what half these options on Filter are even meant to do.","governing_rfc":"RFC-BPR-0032","related_rfcs":["RFC-BPR-0066","RFC-BPR-0028","RFC-BPR-0024"],"related_cards":["BPR-0783","BPR-1567","BPR-1573","BPR-1575"],"reservation":"docs/issues/_reservations/BPR-1580--BPR-1580--b54fb616e5aa.json"}
parked_on: "human-review"
---

# BPR-1580: Home arranges and filters from one View menu whose groups are labelled

The owner said on 2026-10-17, of Home's strip: "I wonder if we should
collapse these 3 into one", and of Filter: "I don't understand what half
these options on Filter are even meant to do". The three are [[BPR-0783]]'s
**Group: None ▾**, **State headers** and **Filter ▾**. Filter's four groups
(My role, Added by, Quiet items, Sort within a group) are `QMenu::addSection`
titles, and the native macOS menu draws a section as a bare separator, so on
the owner's Mac the menu is twenty unlabelled choices. Nothing in it says what
Participant, No role, an observer rule or a quiet item is.

Governing design: Home's strip, Group, State headers and Filter of
[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md)
as [RFC-BPR-0066](../../rfcs/RFC-BPR-0066-home-authored-pull-requests.md)
amends it, the strip of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), and
the per-window view state of
[RFC-BPR-0024](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md).
What each choice does, what is stored and where stays as [[BPR-0783]] left
it; this card changes how the choices are reached, named and summarised.

## Acceptance

- AC1: One control. While Home is shown and an account can fill its list, one
  **View** menu button stands where Group, State headers and Filter stood,
  before the refresh glyph; signed out and on the review strip it is absent.
  Its menu reads, top to bottom: **Group by: ‹value›** ▸ (None, Organization,
  Repository, Account), **Sort by: ‹value›** ▸ (Priority, Repository, Pull
  request number), **State headers** (checkable), a separator, **My role:
  ‹value›** ▸ (Any, Author, Reviewer, Requested, Participant, No role),
  **Added by: ‹value›** ▸ (Any, Repository rule, Author rule, Path rule),
  **Quiet items: ‹value›** ▸ (Show, Hide, Only quiet), a separator, **Reset
  to defaults**, a separator, the footer "Nothing here changes GitHub."
  and **Open Reviews & queue**. Every group's label is an ordinary menu
  item's text, so the native macOS menu draws it; the menu uses no section titles. Each submenu is one
  exclusive choice. A native macOS menu draws no tooltip, so every choice
  whose meaning is not obvious says it in its own text (My role: Any role, I
  opened it, I reviewed it, My review is requested, I took part in it, None of
  these (a rule or a follow added it); Added by: Any way, A rule on a
  repository, an author or a path, after a line saying where observer rules
  are kept; Quiet items opens with what quiet means),
  while the parent item shows the short value; every choice also has a
  tooltip and status tip where the platform draws them.
- AC2: Added by is shown only when the profile has an observer rule, or when
  the window is already filtered by one (so a filter is never out of reach).
- AC3: The button reads **View** at the defaults. When any choice in the menu
  differs from its default it carries the strip's count pill (the one the
  comments button carries) with how many do, and its accessible description
  and tooltip name each such choice. **Reset to defaults** is enabled exactly then and in one
  gesture restores Group None, State headers on, My role Any, Added by Any,
  Quiet items Show and Sort Priority; the profile hears the filter part as it
  hears a filter choice.
- AC4: When the queue has rows and the view hides all of them, the list says
  so in one sentence with a **Clear filters** link that Tab, Return and Space
  reach and that puts My role, Added by and Quiet items back to their
  defaults, instead of an empty page; on Home the line under the bar does not
  repeat it. An empty queue keeps its own sentence.
- AC5: Behaviour is kept: each choice dispatches the same typed queue intent
  and per-window view as before, two windows still filter independently, the
  stored view and preferences are unchanged (no schema change), the button is
  in the strip's Tab order and opens by keyboard, and the submenus are
  reachable by arrow keys and named for assistive technology.
- AC6: Qt and composed tests cover every option reached and applied, labels
  as item text, Added by hidden without rules, the indicator, reset, the
  filtered empty sentence and persistence, with a negative control that fails
  on section-title groups; focused tests and one full `ctest --preset dev` on
  the merged tree, clang-format 22, tidy, the documentation gate and the
  resealed composition contract pass.
- AC7 (human): The owner accepts the frozen candidate in Light and Dark:
  "Home's View menu replaces Group, State headers and Filter, its groups are
  labelled and explained, and a changed view is visible on the button."

## Implementation

Qt presents: `workbench_window` builds the one menu and button; `home_list`
gains the filtered sentence. No core or host change, no QSS, no new stored
state. The composition's strip-order check names the one View action, so the
round-trip composition contract is resealed. BPR-1567's shortcut catalogue
lists none of these controls and no key is added. The other Home cards
([[BPR-1573]]'s Close review, [[BPR-1575]]'s refresh feedback) are not
touched. Never drive or capture the owner's desktop; renders are offscreen and
the owner checks the frozen build themselves.

## Candidate v001 (parked on human-review, 2026-10-17)

- Branch `bpr-1580-home-view-menu`, candidate commit `254e0a1f0` (slice
  `113056194`, merge of main at `e1485e3aa`, reseal), in the worktree
  `/Users/jaeger/.codex/worktrees/bpr1580-home-view/BetterPullReview`. Not
  merged; it lands after the owner accepts. Changed paths:
  `src/adapters/qt/{include/bpr/qt,src}/workbench_window.*`,
  `src/adapters/qt/{include/bpr/qt,src}/home_list.*`,
  `src/app/src/github_application.cpp` (the strip-order check names
  `homeViewAction`), `tests/adapters_qt/workbench_window_tests.cpp`,
  `tests/adapters_qt/composed_github_tests.cpp`,
  `tests/adapters_qt/surface_capture_course_tests.cpp`,
  `tests/end_to_end/profile_window_layout_public_path_tests.cpp`,
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`
  (resealed) and `docs/ux/workbench-specification.md`.
- Design (frontend-design loaded). Submenus rather than disabled title
  items: Qt's platform menu item has no tooltip and no section title, so on
  macOS only an item's text is drawn; a submenu's own item can carry the
  group's name and its value ("My role: Requested"), so the closed menu reads
  as a summary of the view, it is seven items rather than twenty-five, arrow
  keys and VoiceOver reach every group as a named submenu, and nothing is
  drawn dimmed as a heading would be. The cost is one more step to change a
  value that is rarely changed. Because tooltips never show in a native menu,
  each unclear choice says what it means in its own text, and Quiet items and
  Added by open with one dimmed line defining quiet and observer rules. The
  count reuses the strip's own pill (the comments button's), so the strip
  keeps one way of saying "a number here". The footer was shortened to
  "Nothing here changes GitHub." once Added by carried where rules live.
  The filtered sentence colours its link in markup in the sentence's ink,
  because a rich-text anchor takes the application's link colour when parsed
  (the offscreen Dark render first showed it in pure blue).
- Evidence: `qt_workbench_home_discovery` runs
  `test_home_strip_holds_view_and_refresh` and
  `test_home_view_menu_and_its_gestures`: the inventory as a native menu draws
  it (visible non-separator item text), every submenu's words, no section
  title anywhere, the accessible names, each value of each group triggered and
  applied with its typed intent, the count and description (1, 2, 5), Added by
  hidden without a rule and kept while filtered by one, Clear filters by
  Return (three intents, arrangement kept), Reset to defaults (one intent for
  the one changed field), the profile-following window and the owning window.
  Negative control in the same test: a menu grouped by `addSection` titles
  reads to the oracle as unlabelled choices. The surface capture course
  renders closed, open, two submenus, changed and filtered-empty in four
  themes with assertions. Focused runs pass (composed GitHub, composed Home,
  title row, focus order, Home discovery, capture course). Full
  `run_test_suite.py --preset dev` on the merged tree: 758 passed, 11 failed,
  1 skipped (`application_large_review_cache_bytes`, environment); all 11 pass
  when rerun one at a time (six large-review benchmarks under a load average
  near 50, two contracts during a disk-full minute on the shared volume, two
  known flakes, and `rfc0016_production_identity_security`, which refused a
  temporary `.agents` link of mine that pointed outside the worktree).
  Report `build/test-results/dev/20261017T095933.510853Z` in the worktree.
  clang-format 22 clean over every touched file. clang-tidy 22 over the seven
  touched translation units: the only findings are three
  `portability-template-virtual-member-function` lines in `home_list.hpp`'s
  base `home_fitted_view` template, identical in a control worktree without this
  change. `DOCUMENTATION_OK`. BPR-1567's shortcut catalogue lists none of
  these controls and no key was added.
- Frozen root `/Users/jaeger/.codex/attended-builds/bpr1580-view-v001.o_7wpngq`
  from `254e0a1f0` (fixture `github-com`, `--attended-home-look`), executable
  SHA-256 `21d1ae40cd53…`; its `--check` reached
  `ATTENDED_HOME_SHOWN:rows=3`. Launch:
  `python3 /Users/jaeger/.codex/attended-builds/bpr1580-view-v001.o_7wpngq/launch.py`.
- New short session `579c007a8dd4b1baa59c0d089eca606d`, state
  `/Users/jaeger/.codex/attended-review/bpr1580-v001-CbjuXE`, repo root the
  worktree. One Accept/Reject candidate `BPR-1580-view-menu-v001`
  (fingerprint `0fd682e2…`), ready at event cursor 3, with before and after
  renders in Light and Dark (closed, open, My role, Quiet items, changed,
  changed and open, filtered-empty).

## Next

Read session `579c007a…` from cursor 3. Accept satisfies AC7: merge the
branch onto main (reseal again if main's `github_application.cpp` moved),
finish the card, stop the host, release the candidate and remove the frozen
root with `scripts/freeze_attended_build.py remove`. A Reject's note is
v002's change.
