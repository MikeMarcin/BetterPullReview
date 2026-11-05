---
schema: "implement-rfc.card.v1"
id: "BPR-1865"
title: "Home filters to find a pull request"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","found_by":"Owner, 2026-11-04: \"I want to be able to fuzzy filter the home screen to quickly find a PR. Probably needs frontend design\"","reservation":"docs/issues/_reservations/BPR-1865--BPR-1865--ed18fa36eb70.json","related_cards":["BPR-0776","BPR-0783","BPR-1289","BPR-1365","BPR-1580","BPR-1600","BPR-1843"],"frontend_design":"frontend-design:frontend-design loaded before the design; the filter is a quiet line of the lists rather than a second bordered field, and matches sit on a plate of the row's own ink so they claim no state colour. Offscreen renders inspected (not committed).","renders":"build/dev/renders-1865 and build/dev/tests/adapters_qt/home-filter-*.png in the implementing worktree (not committed)"}
acceptance: [{"criterion":"AC1 The core matches and ranks deterministically: number forms (123, #123, name#123), exact number, then prefix and word start, then a run, then scattered letters, typos only as a rescue; ties keep input order; Unicode titles fold","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_home_filter|core_fuzzy_match)$'","detail":"core::match_home_filter and core::rank_home_filter over home_filter_candidate (number, title, repository, author, branch, labels), built on BPR-1289's fuzzy_match; twelve courses: number forms, exact number above every text match, tier order, every field, the title winning a tie, terms across fields and runs in the title, merged spans, input-order ties, accents, CJK, full width and emoji, typo rescue, blank and overlong queries, 2,000 rows."}},{"criterion":"AC2 A filter field on Home under the Open bar, reached by Command-F (Find's key moves to it while Home shows, as Refresh's does), by \"/\" or by typing in any Home list","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_home_filter|qt_keyboard_shortcuts_scale_100|application_home_filter_public_path)$'","detail":"Catalogue: home_filter (Find, On Home, conditional) and home_filter_from_list ('/', read); find is conditional. The catalogue course proves Find's key is bound once, on homeFilterAction, on Home and on sourceFindAction in the review. Typing a character in a list starts the filter with it; '/' only moves the keyboard; Space and modified keys stay the list's."}},{"criterion":"AC3 Every Home section is filtered: the queue panels in every arrangement, Recently viewed, Ignored, the Active review card and Review Cycles; a section with no match is not shown; nothing matching is one sentence","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_home_filter|application_home_filter_public_path)$'","detail":"Each slice admits and orders by the shared ranking; panels count their matches and go when empty; collapsed panels are held open and their own collapse returns without reaching the remembered view; Recently viewed rows hide and are marked; 'Nothing on Home matches “zebra”…' when nothing does. Branch and labels are matched by the core but Home's rows carry neither yet."}},{"criterion":"AC4 Matched characters are marked in the rows","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_home_filter$'","detail":"title_marks, identity_marks and author_marks roles in UTF-16 (é, ë counted as characters); home_row_delegate::mark_rects draws one rounded plate of the row's ink per run and line, and the quiet line's matched characters rise to the full ink."}},{"criterion":"AC5 Arrow keys move through results, Return opens (Command-Return in a new window), Escape clears and then leaves the field","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_home_filter|application_home_filter_public_path)$'","detail":"The best match anywhere is the current result; Down and Up move it in reading order with the keyboard in the field; Return opened #18 through the composed application; a Recently viewed result opens as Recently viewed does; the second Escape puts the keyboard on the list's row."}},{"criterion":"AC6 Instant on large lists, no network","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_home_filter|application_home_filter_public_path)$'","detail":"The composed course logs every account read from before Command-F to Return and finds none. Debug timing printed, not asserted: 1,500 rows filter in tens of milliseconds per keystroke (home-filter-tests:timing)."}},{"criterion":"AC7 Earlier Home courses and gates keep passing","outcome":"pass","revision":"worktree-agent-afb3fc1fdbc0c61bf","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_home_list_scale_100|qt_home_list_scale_200|qt_workbench_focus_order_scale_100|qt_workbench_home_discovery_scale_100|qt_workbench_open_review_from_home_scale_100|qt_workbench_title_row_scale_100|qt_composed_github_scale_100|qt_pending_veil|qt_refresh_feedback_window|qt_home_card_fit_scale_100|application_home_return_public_path|architecture_boundary_contract)$'; _coding-standard/scripts/format.py; _coding-standard/scripts/tidy.py --paths <changed TUs>","detail":"All pass. clang-format 22 applied to every changed file; clang-tidy clean on the new and changed Home translation units. architecture_boundaries fails on main already (QToolTip::showText at workbench_window.cpp ActivityInfo, not this card's). The documentation gate's one error is NEXT_ID BPR-1866 behind a BPR-1867 allocated on another branch, which this card may not touch."}},{"criterion":"AC8 Owner look: the filter's place under the Open bar and its look","outcome":"pass","revision":"296c9fb22","evidence":{"kind":"observation","detail":"Owner Accept, session 8a1fb0fdb47d4fcecf5ab4db5ef01444, decision 61095ad7-3ef8-4d93-9369-7d016f4b8fed (2026-11-05T02:12:17Z, event 8), candidate BPR-1865-filter-field-v001, fingerprint aae770ae58ba64e24cc12689ea553d26898c11a10b7056689a353d77a56af22d, frozen build of 296c9fb22 (executable 9a5ed7f77f21…); no note."}},{"criterion":"AC9 Owner look: how matches are marked and ranked","outcome":"pass","revision":"296c9fb22","evidence":{"kind":"observation","detail":"Owner Accept, session 8a1fb0fdb47d4fcecf5ab4db5ef01444, decision 3e21be61-4cb4-4bf3-8cb6-6a22fc06dafa (2026-11-05T02:12:49Z, event 10), candidate BPR-1865-matches-v001, fingerprint 050223aa3d321234c5616b16ab16ee2211cd91df7b4e2a8aac4c9b94c43c11b4, frozen build of 296c9fb22 (executable 9a5ed7f77f21…); no note."}}]
---

# BPR-1865: Home filters to find a pull request

## Report

The owner, 2026-11-04: "I want to be able to fuzzy filter the home screen to
quickly find a PR. Probably needs frontend design."

## Design

`frontend-design:frontend-design` was loaded before designing.

- **Place.** One quiet line under the Open bar, above everything it filters
  (the Active review card, Review Cycles and every panel of the list). The
  bar names a pull request anywhere; the filter finds one among Home's own
  rows, so it is drawn as part of the lists rather than as a second bordered
  field: a magnifier under the bar's label, the field, and at its right its
  key (⌘F) or, while it filters, "2 matches", over a hairline in the mid ink
  that turns to the text ink, heavier, while it has the keyboard. No state
  colour: on Home colour means state.
- **Keys.** Find's key is the filter's while Home shows and the file's Find
  otherwise, moved as [[BPR-1600]] moves Refresh's. "/" or any character
  typed in a Home list goes to the field. In the field, Down and Up move the
  current result, Return opens it, ⌘Return opens it in a new window, Escape
  clears and then hands the keyboard back to the list.
- **Matches.** A faint plate of the row's own ink under each matched run;
  in the quiet second line the matched characters also rise to the full
  ink. Sections keep their order; rows inside each are ranked best first,
  and the best match anywhere is the current result. A section with no
  match is not shown, and nothing matching is one sentence.

## Built

- **Core** `bpr/core/home_filter.hpp`: `match_home_filter`,
  `rank_home_filter`, `home_filter_active`. Terms across fields, number
  forms, tiers, field order at equal strength, merged spans, typo rescue.
- **Qt** `home_filter_field` (the line), `home_list::set_filter` and its
  result navigation (one ranking shared by every slice model and Recently
  viewed), `home_row_delegate` marks, `workbench_window` wiring (the Active
  review card, Review Cycles, Command-F, clearing on leaving Home),
  catalogue entries `home_filter` and `home_filter_from_list`.
- **Fixture** sixteen titled pull requests
  (`home_filter_fixture_rows`), served by discovery for the composed course
  and, with `--attended-home-rows`, for the attended look.
- **Spec** `docs/ux/workbench-specification.md`, Home › The filter, and the
  shortcut table.

## Left open

- Home's rows carry no head branch or labels, so those fields are empty
  until discovery reads them; the core already matches both.
- Recently viewed keeps its recency order while filtering (its rows are
  addressed by position); only the queue's panels are re-ranked.
- The Active review card is shown or hidden, not marked.

## Owner look — prepared 2026-11-04

- Frozen build of `296c9fb22` with `scripts/freeze_attended_build.py freeze`
  (root `/Users/jaeger/.codex/attended-builds/bpr1865-home-filter-v001.ip_qsfgd`;
  executable SHA-256
  `9a5ed7f77f21f45aaa0523265e3db52e8427c65be0b18aefcd2c6edf9c7e5c0d`), fixture
  `github-com`, `--attended-home-look --attended-home-rows`: the fixture
  account signs in, opens `#17` once, saves its repository and returns to
  Home with 19 rows (sixteen titled ones from `home_filter_fixture_rows`).
  The launcher's own offscreen check reached
  `ATTENDED_HOME_SHOWN:rows=19:delay=0ms`. Launch:
  `python3 /Users/jaeger/.codex/attended-builds/bpr1865-home-filter-v001.ip_qsfgd/launch.py`.
- New short session `8a1fb0fdb47d4fcecf5ab4db5ef01444`, state
  `/Users/jaeger/.codex/attended-review/bpr1865-v001-68jNhO/state`, repo root
  the implementing worktree, host started with `serve --tailscale`. Two
  Accept/Reject candidates, ready at event cursor 6:
  `BPR-1865-filter-field-v001` (AC8, fingerprint `aae770ae58ba…`) and
  `BPR-1865-matches-v001` (AC9, fingerprint `050223aa3d32…`), each with
  offscreen renders of the composed course.
- Next: on hand-back, read `status --after 6`. Each Accept closes its
  criterion; a Reject's note is the next round's change.

## Owner decision and landing

- 2026-11-05, session `8a1fb0fd…`, handed back at event 11: **Accept** of
  `BPR-1865-filter-field-v001` (decision `61095ad7-3ef8-4d93-9369-7d016f4b8fed`,
  event 8) and **Accept** of `BPR-1865-matches-v001` (decision
  `3e21be61-4cb4-4bf3-8cb6-6a22fc06dafa`, event 10), no notes. Host stopped;
  frozen root removed with `scripts/freeze_attended_build.py remove`.
- Merged with main at `a2e2ca244` (BPR-1857, BPR-1866, BPR-1850) without
  conflict; the focused Home courses pass again on the merge.
