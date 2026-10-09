---
schema: "implement-rfc.card.v1"
id: "BPR-1346"
title: "A small gap between each label's icon and its word in the label picker"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"polish","origin":"BPR-1091 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1091-workshop-candidate-line-v002 Accept note","decision_id":"945adb1b-962e-45ec-a113-3534de2187e5","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1091","BPR-1083","BPR-1347"],"acceptance":["BPR-1346:gap","BPR-1346:owner-look"]}
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
