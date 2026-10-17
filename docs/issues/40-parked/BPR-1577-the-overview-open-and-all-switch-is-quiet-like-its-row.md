---
schema: "implement-rfc.card.v1"
id: "BPR-1577"
title: "The Overview's Open and All switch is quiet like its row"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"This particular Open button on the overview page is ugly. I guess it's mostly because it's highlighted like that and nothing else is.","governing_rfc":"RFC-BPR-0049","related_rfcs":["RFC-BPR-0050"],"related_cards":["BPR-1089","BPR-1337","BPR-1350","BPR-1157"],"reservation":"docs/issues/_reservations/BPR-1577--BPR-1577--1dde60b7fa88.json"}
parked_on: "human-review"
---

# BPR-1577: The Overview's Open and All switch is quiet like its row

The owner said on 2026-10-17: "This particular Open button on the overview
page is ugly", and then: "I guess it's mostly because it's highlighted like
that and nothing else is." The switch is [[BPR-1089]]'s Open or All beside
the Overview's Files heading, where the counts start (Open by default), next
to the ? count, the Label key and collapse and expand all folders. Its two
halves are plain auto-raise `QToolButton`s. The style draws a checked one as
a light grey block, the only filled control on the row, and grey on Dark.
Open carries its mark beside the word and All has none, so the two halves
differ in height and in the size of their words.

Governing design: the Overview counts of
[RFC-BPR-0049](../../rfcs/RFC-BPR-0049-label-counters-overview-settings.md)
and the marks of
[RFC-BPR-0050](../../rfcs/RFC-BPR-0050-feedback-ux-label-icon-design.md).
The switch's behaviour, names and keyboard route stay as [[BPR-1089]]
accepted them; only how it is drawn changes.

## Acceptance

- AC1: Open and All are drawn by the page, the same height as the row's
  icon buttons and in the same type size, with no fill or border at rest.
  The chosen half is its word in the text colour and semibold over a
  two-pixel accent underline; the other is its word in the secondary text
  colour. Each half is as wide as its word in semibold, so choosing moves
  nothing. Open keeps the round open mark, at the height of its word and
  centred on it, because the mark is what Open counts.
- AC2: In the four built-in themes the words read at 4.5:1 or more against
  the page and the underline at 3:1 or more; the chosen half differs from the
  other by its underline. A render check covers both states in every theme,
  with a negative control that fails on the old grey block.
- AC3: Behaviour is unchanged: object names, accessible names and
  descriptions, tooltips, Tab order, Space and the dispatched mode are as
  before, and the keyboard focus ring shows on the focused half.
- AC4: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark: "Open
  and All read as a quiet part of the Files row, with the chosen one clear".

## Implementation

Qt presents: a page-local checkable tool button paints its word, mark and
underline from the page palette; the indicator colour is the window's focus
indicator floor over the page (`focus_ring_colour`). No QSS, no new state,
no core or host change. The UX redesign of the drawers, the Agent tab and
Compose review and the loading-states card are not touched. Never drive or
capture the owner's desktop; the owner checks the frozen build themselves.

## Candidate v001 (parked on human-review, 2026-10-17)

- Branch `bpr-1577-quiet-counts-switch`, commit `4f559bbc1`, in the worktree
  `/Users/jaeger/.codex/worktrees/bpr1577-counts-switch/BetterPullReview`.
  Not merged; it lands after the owner accepts. Changed paths:
  `src/adapters/qt/src/pull_request_overview_page.cpp` (a page-local
  `counts_mode_button`), `tests/adapters_qt/pull_request_overview_page_tests.cpp`
  (`test_counts_switch_is_quiet_in_every_theme`, also run alone with
  `--switch`) and `docs/ux/conventional-comments-feedback.md`.
- Design (frontend-design loaded): no fill or border at rest; the chosen
  word in the text colour and semibold over a two-pixel underline in the
  window's focus-indicator colour (`focus_ring_colour` over `Window`, the
  accent lifted to 3:1), as the review bar's navigation rails mark a group;
  the other word in the text colour a quarter of the way to the page. Both
  halves take the height Qt gives a tool button with a 16 px icon, so they
  match the ? count and the Label key. Open keeps the round open mark, drawn
  as tall as the capitals. A ring shows on keyboard focus, not on a click.
- Keyboard route unchanged and now tested: Tab reaches the chosen half,
  Right moves to the other, Space chooses it, Tab leaves the switch (Qt's
  route for an auto-exclusive group).
- Evidence: the switch reading passes in light, dark and both high-contrast
  themes at 100%, 125% and 200%. The underline measures 3.00 to 3.02:1 and the
  words 4.5:1 or more. Its control, the switch as the style drew it, fails in
  every theme on three counts: fill or border, no 3:1 underline, and All
  22 px beside the row's 23 px. `qt_pull_request_overview_page`, chips at
  125% and 200%, folder rows and read marks pass (6 of 6). clang-format 22
  passes. clang-tidy 22 finds nothing new; the 12 findings it reports in the
  test file are in base lines. The documentation gate prints
  `DOCUMENTATION_OK`.
- Frozen root `/Users/jaeger/bpr1577-counts-switch-v001.ho7h3gkb` from
  `4f559bbc1` (fixture `overview-labels-v1`, `--open-pull-request`),
  executable SHA-256 `ada14b6992d6…`; its `--check` reached
  `ATTENDED_PULL_REQUEST_OPENED`. Launch:
  `python3 /Users/jaeger/bpr1577-counts-switch-v001.ho7h3gkb/launch.py`.
- New short session `eb17719250955a0cda2ac2c6cc9b991c`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1577-counts-switch-v001`,
  repo root the main checkout, host detached; offline page `review.html` in
  the state directory. One Accept/Reject candidate
  `BPR-1577-counts-switch-v001` (`BPR-1577:owner-look`), ready at event
  cursor 3, with before and after renders in Light and Dark and a four-theme
  sheet of both states.

## Next

Read session `eb177192…` from cursor 3. Accept satisfies AC5: merge
`4f559bbc1` onto main, finish the card, stop the host, release the candidate
and remove the frozen root with `scripts/freeze_attended_build.py remove`. A
Reject's note is v002's change.
