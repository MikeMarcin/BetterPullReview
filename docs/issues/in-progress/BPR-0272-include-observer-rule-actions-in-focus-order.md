---
id: BPR-0272
title: Include observer-rule actions in the workbench focus order
status: In Progress
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
