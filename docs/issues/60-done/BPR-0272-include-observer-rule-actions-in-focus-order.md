---
id: BPR-0272
title: Include observer-rule actions in the workbench focus order
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0271]
---

# Include observer-rule actions in the workbench focus order

The review-queue surface creates focusable tool-button presenters for Add local
rule and Remove selected rule, but the workbench discards their widget
identities and omits both from `focus_order()`. The declared shell sequence
therefore jumps from Exclude to technical details even when one or both rule
actions are available to a keyboard user.

## Required final state

- The workbench retains the exact Add and Remove tool-button presenters and
  includes every enabled presenter in the declared shell focus sequence after
  the observer-rule inputs and before pull-request technical details.
- Both presenters have stable widget identities, explicit accessible names and
  descriptions, and appear once in the real Qt tab chain when available.
- Add availability remains owned by queue readiness, a configured typed rule
  dispatcher, and a valid trimmed pattern. Remove availability remains owned
  by readiness, dispatch, and an accepted selected persisted rule.
- Disabled Add or Remove presenters are omitted from available-stop evidence;
  restoring their accepted prerequisites restores their exact focus position.
- Existing semantic rule gestures still emit only typed local intents and do
  not mutate the queue before an accepted projection.

## Non-goals

- Do not change observer-rule validation, persistence, enrollment, or GitHub
  roles and assignments.
- Do not infer button widgets through `QAction::associatedObjects()` or add a
  second focus-order authority in tests.
- Do not broaden this card into child-surface internal focus stops or weaken
  BPR-0271's declared-stop/live-chain evidence.
- Do not execute a live target, use a credential, or claim Windows/Linux
  runtime evidence.

## Verification

- Rebuild and run workbench-window and composed GitHub Qt selectors at 100,
  125, and 200 percent, plus review-queue and queue-composition contracts.
- Prove Exclude -> Add -> Remove -> technical details in the declared and real
  Qt chain when both actions are available.
- Prove blank input and absent persisted selection omit only the corresponding
  disabled presenters, and accepted prerequisites restore them without
  duplicate stops.
- Run scoped format/tidy, documentation, diff, and the final macOS `dev`
  continuation.
- Independently review hostile omitted, duplicated, disabled, and reordered
  presenter counterexamples.

## Closeout

The workbench now retains the exact Add and Remove tool-button presenters,
gives each a stable object identity and explicit accessible name and
description, and declares both immediately after Exclude. Available-stop
evidence preserves the structural order while omitting only disabled controls.
The focused tests prove blank input, absent selection, restored prerequisites,
single-stop identity, typed local intents, and no queue mutation before an
accepted projection. The composed 100, 125, and 200 percent courses walk the
real Qt chain through Exclude, Add, Remove, and technical details.

The exact eight-test matrix passed under the checked-in macOS network-denial
wrapper: all three workbench scales, all three composed GitHub scales,
`qt_review_queue`, and `application_queue_composition_contract`. Scoped format
and strict clang-tidy passed, as did the documentation and diff gates.
Independent closeout review returned `ACCEPT` with no blocker or high-risk
finding; its only verification gap was an outer-sandbox refusal before product
execution, covered by the primary escalated 8/8 replay.
