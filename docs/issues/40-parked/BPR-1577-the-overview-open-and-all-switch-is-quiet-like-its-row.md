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

- AC1 (revised after the owner's v001 Reject): Open and All are drawn by the
  page, so the platform style does not decide their look. They take the
  quiet chip of Qt's Fusion style, which the owner liked: the chosen half is
  a softly filled rounded box with a one-pixel border, and the other half is
  its word alone. Both are the row's icon-button height, in the same type
  size, weight and text colour. Open keeps its round open mark in a 16 px box.
- AC2: In the four built-in themes the words read at 4.5:1 or more against
  the page, the chosen half's border reads at 3:1 or more, and its fill stays
  a tint (1.35:1 at most), never a block. The other half has no fill or
  border. A render check covers both states in every theme, with a negative
  control: the switch as the style drew it.
- AC3: Behaviour is unchanged: object names, accessible names and
  descriptions, tooltips, Tab order, Space and the dispatched mode are as
  before, and the keyboard focus ring shows on the focused half.
- AC4: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark: "Open
  and All look like the quiet Fusion chip and read as a calm part of the
  Files row".

## Implementation

Qt presents: a page-local checkable tool button paints its chip, word and
mark from the page palette (v002); its keyboard ring is the window's focus
colour (`focus_ring_colour`). No QSS, no forced style, no new state,
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

- Full `cmake --build --preset dev` and `ctest --preset dev` of `4f559bbc1`
  (rebased on main `007b8f093`): the build passes and ctest passes 761 of 770.
  Seven of the nine failures pass when run alone. Three failed only because
  the worktree has no `.agents/` (documentation_structure,
  documentation_issue_graph_contract, issue_id_allocation_contract). Four
  failed under load from 13 agents (designated_live_macos_capsule_contract,
  live_mutation_isolation, application_anonymous_public_preview_unavailable,
  application_label_authoring_contract). The other two,
  application_composition_contract and
  application_review_round_trip_composition_contract, fail on main too: the
  sealed `github_application.cpp` digest drifted with BPR-1571's `fbec1f08c`,
  and this branch touches neither that file nor the contracts. The dev build
  tree was then removed to free disk.

## Owner verdict v001: Reject (2026-10-17)

Decision `553feea3-ef63-4d87-833a-a2acebaa90dc`, session `eb177192…` event 4:
"I'm confused because the before here actually looks quite good to me and is
totally different from the actual current appearance as I know it." The note
attached a screenshot of the app today (Dark, macOS style): Open is a light
grey filled block with a large amber disc, and All sits in a dark box of its
own. v001's "before" was an offscreen Fusion render, so the comparison
misled. The owner liked Fusion's look: a soft bordered chip round the chosen
half and a bare word for the other.

A native render (cocoa platform, page drawn with `Qt::WA_DontShowOnScreen`,
foreground transform off, nothing shown or captured) was not faithful either.
Without a real window the macOS style draws every tool button as a bevelled
box. The owner's screenshot is therefore the before. The v001 host was
stopped, its candidate released with the verdict kept, and its frozen root
removed.

## Candidate v002 (parked on human-review, 2026-10-17)

- Commit `7066b48db` on `bpr-1577-quiet-counts-switch`, one slice on main
  `30eb69efa`; not merged. It changes the same three paths as v001.
- Design: Fusion's chip, drawn by the page. The fill is 7% of the way from
  the page to the text. The border is the page colour moved towards the text
  just far enough for 3:1, against Fusion's about 2:1, and the radius is 3.
  Both words are in the text colour at their ordinary weight, and Open's mark
  is in a 16 px box. A hover or press is only a soft fill, and a ring shows on
  keyboard focus. `--switch-render ROOT` saves the row in both states in
  whatever style runs.
- Evidence: the chip reading passes in the four themes at 100%, 125% and
  200%, including the keyboard route. The control (the style-drawn switch)
  fails it in every theme. The six Overview ctest rows pass, as do
  clang-format 22 and the documentation gate (`DOCUMENTATION_OK`). clang-tidy
  22 finds nothing new; it reports the same 12 findings in base lines. The
  full dev ctest above was v001's tree. It runs again on the merged tree at
  landing, after BPR-1585 lands in the same files.
- Frozen root `/Users/jaeger/bpr1577-counts-switch-v002.4iietnlb` from
  `7066b48db`, executable SHA-256 `541a78d87004…`; its check reached
  `ATTENDED_PULL_REQUEST_OPENED`. Launch:
  `python3 /Users/jaeger/bpr1577-counts-switch-v002.4iietnlb/launch.py`.
- New short session `cf892a3b42c656c51e88fe0830a2936f`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1577-counts-switch-v002`,
  host detached, offline `review.html` in the state directory. One
  Accept/Reject candidate, `BPR-1577-counts-switch-v002` (fingerprint
  `5676d27746e7…`), ready at cursor 3. The session names how each picture
  was made: the owner's own screenshot, the Fusion look they liked, and v002
  offscreen at 2x. It points at the frozen build as the source of truth.

## Owner verdict v002: Accept (2026-10-17)

Decision `296a1893-d1bc-4010-8463-512fc66a0f7b`, session `cf892a3b…` event 4,
with no note, on candidate `BPR-1577-counts-switch-v002` (fingerprint
`5676d27746e7…`, revision `frozen-7066b48db-v002`). The owner handed the
session back at event 5, and the cursor was read to 5. This satisfies AC5.

## Next

Landing waits for BPR-1585, which is landing first in the same files. A trial
rebase of the candidate onto `bpr-1585-files-band` applied without conflicts.
The candidate is now `1b3670d1a` on `bpr-1577-quiet-counts-switch`, rebased
onto main `db49949e1`, and its full dev tree is built. When BPR-1585 is on
main: rebase, run the focused Overview rows, one full `ctest --preset dev`
and the documentation gate, merge, finish the card Done and push. Then stop
the host (PID 39316), release the candidate, remove the frozen root
`/Users/jaeger/bpr1577-counts-switch-v002.4iietnlb` and the worktree.
