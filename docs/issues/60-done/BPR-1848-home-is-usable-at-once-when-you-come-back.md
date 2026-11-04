---
schema: "implement-rfc.card.v1"
id: "BPR-1848"
title: "Home is usable at once when you come back to it, and reads only what it must"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"performance","found_by":"Owner, 2026-11-03: \"I hate how slow the home refresh is every time I finish with a PR and return there. Is there any way to speed this up?\"","reservation":"docs/issues/_reservations/BPR-1848--BPR-1848--029e3a417668.json","related_cards":["BPR-0990","BPR-1381","BPR-1576","BPR-1679","BPR-1680","BPR-1681","BPR-1816","BPR-1842"],"frontend_design":"frontend-design:frontend-design loaded before the presentation change; no new visual language: Home's refresh over listed rows takes BPR-1576's accepted partial plate instead of the wash. Offscreen renders inspected, light and dark.","measurement":"Release (qualification-release), macOS arm64, offscreen, public-path fixture holding every account read 330 ms (BPR-1842's measured gh round trip), Home of 12 pull requests in one saved repository; before and after on the same course."}
acceptance: [{"criterion":"AC1 Coming back to Home shows it with its rows at once and never washes over or blocks them while any refresh runs","outcome":"pass","revision":"0cacdad49","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^application_home_return_public_path$'","detail":"Back to Home with a fresh list, Back to Home with an aged list, and Close review: Home shown with both rows usable in 11-16 ms; the list washed 0 ms (before: 930 ms of a 1,339 ms refresh, all of it after the first 400 ms); no model reset and no row removed on any return."}},{"criterion":"AC2 A fresh list reads only the pull request left; an aged list is refreshed once, that row first; a refresh in progress reads it next","outcome":"pass","revision":"0cacdad49","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(core_home_open|application_review_queue_enrollment|application_home_return_public_path)$'","detail":"core::decide_home_return decides; personal_discovery_host::returned reads one retained row without a search or a refreshing status (2 requests, settled at 669 ms), keeps a Refresh asked meanwhile for after it, refreshes an aged list with the row left first, and moves the row left to the front of a running refresh's queue. Close review from Home reads nothing more."}},{"criterion":"AC3 Earlier Home courses keep passing with the new presentation","outcome":"pass","revision":"0cacdad49","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(application_refresh_feedback_public_path|application_home_open_feedback_public_path|application_pending_states_public_path|application_personal_discovery_public_path|application_progressive_open_public_path|qt_pending_veil|qt_refresh_feedback_window)$'","detail":"Two assertions that required the wash now require the partial plate: refresh-feedback's 'Home's list is veiled while it is refreshed' and BPR-1679's 'its list is greyed while it loads'. BPR-1679's course waits for the return's read to end before its measured refresh."}},{"criterion":"AC4 Gates","outcome":"pass","revision":"0cacdad49","evidence":{"kind":"command","command":"_coding-standard/scripts/format_check.py; _coding-standard/scripts/tidy.py --paths <changed TUs>; ctest -R '^architecture_boundaries$'; verify_documentation.py","detail":"clang-format 22 passes on the 14 changed files; architecture_boundaries and architecture_boundary_contract pass; clang-tidy 22 over the 8 changed translation units reports only three findings on lines this card did not change (finish_choice, go_on_to_reviewers naming; a null-pointer path in the fixture at line 5530). The documentation gate reports one error, NEXT_ID BPR-1849 rewinding a high water of BPR-1851 allocated on other branches, which this card may not touch."}}]
---

# BPR-1848: Home is usable at once when you come back to it, and reads only what it must

## Report

The owner, 2026-11-03: "I hate how slow the home refresh is every time I
finish with a PR and return there. Is there any way to speed this up?"

## Measured

A new course, `application_home_return_public_path`, comes back to Home
three ways through the composed application on the public-path fixture:
Back to Home a moment after the list was refreshed, Back to Home once the
list has aged past discovery's five-minute freshness window (a fixture offset
on discovery's clock), and Close review from Home over the open review. The
fixture holds every account read 330 ms (`EveryReadEverywhere`, so Home's
discovery pays a round trip as a pull request's load does) and logs it. The
course prints when Home was shown, when its rows were usable (shown, listed,
not washed, not blocked), how long the list was washed, when discovery was at
rest, what was read, and what the queue's model did. `--measure` only prints;
`--extra-rows N` lists more pull requests.

Release, 12 pull requests, milliseconds from the gesture:

| Return | | Shown | Usable | Washed | At rest | Reads |
|---|---|---|---|---|---|---|
| Back to Home, fresh list | before | 8 | 8 | 0 | 8 | 0: the row left is not read |
| | after | 13 | 13 | 0 | 669 | 2: that row's pull request and reviews |
| Back to Home, aged list | before | 12 | 12, washed from 400 | 930 | 1,338 | 4: 2 retained batches, 2 searches |
| | after | 11 | 11 | 0 | 1,339 | 4, the row left first |
| Close review | before | 17 | 17 | 0 | 17 | 0 |
| | after | 14 | 14 | 0 | 14 | 0 |

No return reset Home's model or removed a row, before or after.

## Root causes

- **Home greyed itself for the whole refresh.** Home's list presented a
  discovery refresh over rows it already listed as `pending_look::refreshing`,
  which after 400 ms washes the whole list in the window's ground (the owner
  in [[BPR-1679]]: "even though its greyed out"). Coming back after more than
  five minutes in a pull request, the usual case, starts a full refresh, so
  Home looked unusable for all of it. On the fixture that is 0.9 s; on the
  owner's GitHub Enterprise server [[BPR-1680]] measured 0.65 s a request and
  estimated a refresh at a couple of hundred requests, so the list stayed
  greyed for tens of seconds to minutes. The rows were in fact clickable
  throughout ([[BPR-1679]]); they did not look it.
- **The row just left was not read when the list was fresh.** Inside the
  freshness window a return read nothing, so a pull request the reviewer had
  just reviewed and closed ([[BPR-1816]] closes the review and shows Home)
  kept the state of the window's last read of it until the next refresh, or a
  manual Refresh, which greyed the whole list again.
- **An aged list was read in queue order**, so the row left could be among
  the last to change.
- Not causes: Home is shown within a frame or two; the queue model is applied
  by identity, with no reset or removal; the five-minute freshness window
  already spared a quick return its refresh.

## Built

- **Core** (`home_open`): `decide_home_return(home_return_facts)` →
  `home_return_read`: nothing (no account, a rate limit, or a fresh list
  without the row), `left` (a fresh list: only the row left), `everything`
  (an aged list), `left_next` (a refresh is running). Pure; tested in
  `core_home_open`.
- **Application** (`personal_discovery_host::returned`): asks the core, then
  reads one retained row alone, with no search and no change to the list's
  status, freshness or Refresh (a Refresh, or an activation that finds the
  list stale, asked meanwhile runs once it ends); or starts the refresh with
  that row first; or moves its queued read to the front of the running
  refresh. Statistics count `ReturnReads` and `ReturnPromotions`.
- **Qt** (`workbench_window`): remembers the pull request the review surface
  last showed, as the queue names it, and on arriving at Home from it gives
  it to `set_home_return_dispatch` instead of the plain activation. Home's
  discovery over listed rows is presented on BPR-1576's partial plate at the
  list's foot ("Refreshing my pull requests in 1 saved repository…"): the
  rows keep their ink, the plate turns, and a list with nothing yet still
  shows the first-load placeholders.
- **Fixture**: `github_read_delay_fixture::EveryReadEverywhere` holds and logs
  every account read, Home's as `home:<operation>`;
  `github_public_path_fixture_options::DiscoveryWallOffsetSeconds` ages
  discovery's clock.

The rate-limit cost of a return is one retained read (two requests) when the
list is fresh, and unchanged when it is not. The freshness window is
unchanged, and no conditional request is added ([[BPR-1680]] option 7 was not
approved).

## Left open

- Settling an aged list still costs the full refresh, read one request at a
  time on the `gh` lane. Running Home's reads beside the window's or batching
  them further is [[BPR-1680]]'s and [[BPR-1842]]'s open design question.
- The pull request left moves to its new place on return because the queue
  takes the window's own read of it then. `review_landing_model::apply`
  reports that one relocation as one `rowsMoved` per row it passes (11 for a
  12-row list); views keep their place, but a single move would be cheaper.
- `test_network_isolation_manifest` fails in this partial build tree on
  `gh_cli_git_smart_transport`'s registration, which this card does not touch.
