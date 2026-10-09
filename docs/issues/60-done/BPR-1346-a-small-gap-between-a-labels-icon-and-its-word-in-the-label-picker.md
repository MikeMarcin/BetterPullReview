---
schema: "implement-rfc.card.v1"
id: "BPR-1346"
title: "A small gap between each label's icon and its word in the label picker"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"polish","origin":"BPR-1091 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1091-workshop-candidate-line-v002 Accept note","decision_id":"945adb1b-962e-45ec-a113-3534de2187e5","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1091","BPR-1083","BPR-1347"],"acceptance":["BPR-1346:gap","BPR-1346:owner-look"]}
acceptance: [{"criterion":"BPR-1346:gap","outcome":"pass","revision":"5fd30f04f","evidence":{"kind":"command","path":"tests/adapters_qt/feedback_label_controls_tests.cpp","detail":"qt_feedback_label_controls measures mark-to-word ink gap on the chip, Plain and all twelve label buttons in light and dark: before 2/3/2 px (fails), after 6/7/6 at 1x and 6.5/7/6.5 at 2x. Focused rows 28/28 at 2f9682d21 and 8/8 at e5063a79c; DOCUMENTATION_OK. macOS."}},{"criterion":"BPR-1346:owner-look","outcome":"pass","revision":"e5063a79c","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1346-a-small-gap-between-a-labels-icon-and-its-word-in-the-label-picker.md","detail":"Owner Accept, no note, 2026-10-09T13:02:25Z: session 41bb446e7df8954d6162fe3592b78cec, candidate BPR-1346-label-gap-v001 (fingerprint 8d6ce31bd0ff..., frozen-e5063a79c-v001), decision 27abab91-83e3-4dbf-936b-f596ece70c56 at event 7, handed back at 9, released at 10."}}]
---

# BPR-1346: A small gap between each label's icon and its word in the label picker

## Owner note

In attended session `0d079940affe6da5c393df07d82a22d8` the owner accepted
[[BPR-1091]]'s workshop candidate line (`BPR-1091-workshop-candidate-line-v002`,
decision `945adb1b-962e-45ec-a113-3534de2187e5`, event 10, 2026-10-09T04:07:11Z)
with two followups. This card is the first, verbatim:

> followup: Can we add like a tiny space between the icon and the word?

The attachment that goes with it shows the label picker open under the
review draft's "question" chip: "Plain comment ⌘0", then the twelve labels in
three columns (praise, nitpick, suggestion, issue, todo, question, thought,
chore, note, typo, polish, quibble), each a button with its coloured mark and
then its word and shortcut (`:pr:`, `:ni:` and so on), then the Adds row
(blocking, non-blocking, if-minor) and the "Your own, such as security" field.
In every button the mark's right edge touches the first letter of the word.
The second followup, Dif's dead space on Discuss, is [[BPR-1347]].

## What exists

- `src/adapters/qt/src/feedback_label_controls.cpp`: `feedback_label_menu`
  builds Plain and the label buttons as `QToolButton`s with
  `Qt::ToolButtonTextBesideIcon` and `feedback_mark_size` icons; the review
  bar's chip (`feedback_label_controls::Chip`) and the completion list's items
  draw the same mark beside their text. The style leaves no gap between a
  tool button's icon and its text on macOS.

## Scope

- A small, fixed gap (a few logical pixels, scaled) between the mark and the
  word on Plain, every label button and the chip, in light and dark, at 1x and
  2x. If the completion list's rows touch too, give them the same gap.
- Nothing else in the picker moves: column widths, order, shortcuts and keys
  stay.

## Acceptance

- `BPR-1346:gap`: a Qt test measures the gap between the drawn mark and the
  text's first ink on Plain, a label button and the chip, and fails against
  the code before the change.
- `BPR-1346:owner-look`: the owner accepts the picker with the gap in one
  short attended session on a frozen build (one Accept/Reject).

Load `frontend-design:frontend-design` before the visual change.

## What changed (`5fd30f04f`, on main as `e5063a79c`)

Design (frontend-design loaded): the gap belongs to the mark, so it holds in
every style and scales with it. `feedback_label_controls.cpp` gives the
chip, Plain and the twelve label buttons an icon 4 logical pixels wider than
the 16-pixel mark, with the mark at its left and the rest transparent
(`mark_icon`, `mark_icon_size`). Under the macOS style the word now starts 4
pixels after the mark (it touched before); the offscreen style draws 6 to 7.
Order, shortcuts, keys and the grid are unchanged; each button is 4 pixels
wider. The completion list's rows already leave about 5 pixels (rendered)
and are unchanged. `docs/ux/conventional-comments-feedback.md` records the
rule.

## Evidence (`BPR-1346:gap`, pass)

- `qt_feedback_label_controls` gained
  `test_a_small_gap_between_the_mark_and_the_word`. It measures, on the
  composed editor's chip, Plain and every label button, the logical columns
  between the mark's last ink and the word's first ink: the mark's ink is
  where the button differs from itself drawn with a blank icon, the word's
  where it differs from itself drawn with transparent text, so the style's
  frame cancels out. It asserts at least 4 and at most 8 pixels, the twelve
  labels within a pixel of each other (each first letter has its own
  bearing), in the product's light and dark profiles.
- Before the change (the source before `5fd30f04f`): chip 2, Plain 3, praise 2, so the
  row fails ("the chip leaves a small gap"). After: chip 6, Plain 7, praise 6
  at 1x and 6.5, 7, 6.5 at 2x, light and dark.
- Renders: the picker and the chip, light and dark, at 1x and 2x (offscreen,
  forced style), inline in the session.
- Rows at `2f9682d21` (28 of 28): `qt_feedback_label_controls`,
  `qt_dif_workshop_placement`, `qt_review_draft_editor`,
  `qt_composed_github_scale_100/125/200`, `qt_workbench_window_scale_100/125/200`,
  `qt_workbench_line_draft`, `qt_workbench_focus_order_scale_100/125/200`,
  the thirteen `application_agent_finding_*_contract` courses,
  `architecture_boundaries` and `coding_format_check` (clang-format 22.1.8).
  Rebased on `2f960c927` as `e5063a79c`: 8 of 8 again
  (`qt_feedback_label_controls`, `qt_dif_workshop_placement`,
  `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`, two
  workshop courses, `architecture_boundaries`, `coding_format_check`).
  `verify_documentation.py`: `DOCUMENTATION_OK`. No pinned source changed.
  macOS only.

## Owner look, session v001 (accepted, below)

One new short session, two candidates, one Accept or Reject question each
(shared with [[BPR-1347]]).

- Session `41bb446e7df8954d6162fe3592b78cec`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1346-1347-v001`,
  repo root the main checkout, ready and protected at event cursor 6. The host
  was started detached and answered on loopback; the offline page is
  `review.html` in the state directory.
- Candidate `BPR-1346-label-gap-v001` (revision `frozen-e5063a79c-v001`, fingerprint
  `8d6ce31bd0ff…`), criterion `BPR-1346:owner-look`, with offscreen renders inline.
- Frozen root `/Users/jaeger/bpr1346-1347-v001.wegp_apd`, made by
  `scripts/freeze_attended_build.py freeze` from `e5063a79c` (fixture
  `conventional-comments-v1`, file `src/engine/renderer.cpp`, the
  `--attended-workshop-agent` look with the fixture agent, ready line
  `ATTENDED_WORKSHOP_SHOWN`). Its own `--check` reached
  `ATTENDED_WORKSHOP_SHOWN` on head `b20cb443…`; executable SHA-256
  `804f06b44c3970811e96a2f782caeae00dc41c26802573a896d327afb1f7312f`.
- Launch: `python3 /Users/jaeger/bpr1346-1347-v001.wegp_apd/launch.py`.

## Next

Read session `41bb446e…` from cursor 6. Accept on `BPR-1346-label-gap-v001` satisfies
`BPR-1346:owner-look` and the card finishes; a Reject comes back with the change in its
note. When both cards are settled, remove the root with
`freeze_attended_build.py remove`. Windows and Linux runs of the rows are
owed to the stations.

## Owner verdict, session v001

Accept, 2026-10-09T13:02:25Z, decision `27abab91-83e3-4dbf-936b-f596ece70c56` at session event 7, no note
(candidate `BPR-1346-label-gap-v001`, fingerprint `8d6ce31bd0ff…`, revision
`frozen-e5063a79c-v001`). The owner handed session
`41bb446e7df8954d6162fe3592b78cec` back at event 9, read from cursor 0;
[[BPR-1347]]'s `BPR-1347-dif-workshop-row-v001` was accepted in the same hand-back.

This satisfies `BPR-1346:owner-look`. `BPR-1346:gap` stands on its recorded
evidence, and the Linux and Windows station runs below pass its rows. The
card is Done.

The host (pid 66656) was stopped and both candidates released with
`resume-development --all` at event 10. The frozen root
`/Users/jaeger/bpr1346-1347-v001.wegp_apd` was removed with
`scripts/freeze_attended_build.py remove`. The session's state folder keeps
the record.

## Linux station run (bpr-main-linux, 2026-10-09)

At `b0ef8d7b9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner: `qt_feedback_label_controls` and `qt_dif_workshop_placement` pass.

## Windows station run (bpr-main-win, 2026-10-09)

At `7672d5a8b`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `qt_feedback_label_controls` and `qt_dif_workshop_placement`.
