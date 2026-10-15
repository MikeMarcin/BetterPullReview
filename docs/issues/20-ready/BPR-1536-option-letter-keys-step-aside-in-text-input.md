---
schema: "implement-rfc.card.v1"
id: "BPR-1536"
title: "Option-letter window and menu keys step aside in text input on macOS (Option-R types ®)"
status: "Ready"
rfcs: []
depends_on: ["BPR-1531"]
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"owner request 2026-10-15 (relayed by the coordinator)","owner_note":"Fix: Option-R (Compose review) in a comment box opens Compose review instead of typing ®. Make it step aside in every text-input widget with the BPR-1531 typing-widget rule, shared, and audit every other Option shortcut in the window and menu bar.","governing_rfc":"RFC-BPR-0052","related_rfcs":["RFC-BPR-0074","RFC-BPR-0028"],"related_cards":["BPR-1129","BPR-1295","BPR-1503","BPR-1531"],"reservation":"docs/issues/_reservations/BPR-1536--BPR-1536--f4c3a47480ed.json","branch_base":"bpr-alt-a-accept (BPR-1531 candidate a51990fd7); shares its takes_typed_text rule and key claim"}
---

# BPR-1536: Option-letter window and menu keys step aside in text input on macOS (Option-R types ®)

While probing [[BPR-1531]], Option-R pressed in a comment editor inside the
window opened Compose review instead of typing `®`. The owner asked on
2026-10-15 to fix it: Option-R steps aside in every widget that takes typed
text, using the same typing-widget rule [[BPR-1531]] built for Option-A, and
every other Option shortcut in the window and the menu bar is audited the
same way and fixed if it steals typing.

Governing design: [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
assigned Option-R and Option-C; [RFC-BPR-0074](../../rfcs/RFC-BPR-0074-header-source-navigation.md)
states the rule this card applies to all of them: "Editable fields and
composers retain their native Option/Alt text input". On macOS Option with a
character key types a character (`®`, `ç`, `å`, `†`, `¡`) or starts a dead
key (`˜` before `n`, `´`, `¨`, `ˆ`); the Cocoa plugin offers such a key to
Qt's shortcut map before the text input system, and AppKit's native menu bar
takes a key equivalent before Qt sees the key at all ([[BPR-1295]]).

Depends on [[BPR-1531]]: the branch starts from its candidate and shares its
`takes_typed_text` rule and key claim, so it lands after [[BPR-1531]] is
accepted.

## Acceptance

- AC1: With keyboard focus in any widget that takes typed text in the window
  (editable line, multi-line editor, editable combo box, Review Cycle
  suggestion editor), every window, button and menu-bar Option+character
  shortcut is offered to the editor first on macOS: none fires, and the
  editor receives the character. This holds for each Option+character
  binding the audit lists, with each enabled. A check against the
  pre-change window fails for Option-R.
- AC2: Outside text input the same keys still work: Option-R opens Compose
  review from the code or a button. Menu-bar items keep their keys in the
  menu, and while a text widget has focus the native menu bar does not carry
  an Option+character key equivalent, restored when focus leaves; tooltips
  keep naming the key throughout.
- AC3: The audit lists every Option binding in the window and the menu bar
  with its result; Option with a non-character key (arrows, Page Up/Down,
  Return) and Command-Option keys are unchanged, and the find field's
  Option-W and Option-L still toggle Whole Word and Find in Selection.
- AC4 (human): The owner accepts the frozen candidate: "⌥R types ® in comment
  boxes and still opens Compose review elsewhere".

## Implementation

Qt presents: `workbench_window` only, no core, host, provider or storage
change. Windows and Linux keep Alt+letter as command keys in editors (text
input there is AltGr, which [[BPR-1336]] already yields); the rule is macOS
only. Never drive or capture the owner's desktop; the owner checks the frozen
build themselves.
