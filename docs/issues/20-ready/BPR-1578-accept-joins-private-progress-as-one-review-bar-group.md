---
schema: "implement-rfc.card.v1"
id: "BPR-1578"
title: "Accept joins private progress as one review-bar group and reads as a button"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"Accept is a button next to other similar text labels in Changes and Files which are not buttons, it's kind of confusing. It also doesn't have the under bar that connects it to the dropdown control like Changes and Files do to their controls.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052","RFC-BPR-0007"],"related_cards":["BPR-1507","BPR-1515","BPR-1519","BPR-1531","BPR-1331"],"reservation":"docs/issues/_reservations/BPR-1578--BPR-1578--5ed4b95b967f.json"}
---

# BPR-1578: Accept joins private progress as one review-bar group and reads as a button

The owner, on 2026-10-17, about the review bar (the second row: "File 5 of
17 « »", "Changes 2 ‹ ›", "Accept", then private progress's state menu):

> Accept is a button next to other similar text labels in Changes and Files
> which are not buttons, it's kind of confusing. It also doesn't have the
> under bar that connects it to the dropdown control like Changes and Files
> do to their controls.

File and Changes are plain labels, each over an accent rail that joins it to
its arrow pair ([[BPR-1519]]). Accept ([[BPR-1507]], Option-A since
[[BPR-1531]]) is a command, but it is drawn as a word in the same type beside
those labels, and it has no rail joining it to the private-progress menu it
belongs with.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
review bar as [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
laid it out, recorded in the [workbench specification](../../ux/workbench-specification.md).
The receipt semantics of [RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)
are unchanged.

## Acceptance

- AC1: The file-progress controls read as one group, as File and Changes do:
  one accent rail, drawn as theirs is, runs under Accept and the private
  progress menu from Accept's left edge to the menu's right edge, and stops
  between this group and the Changes group. It holds at the design width, at
  the width floor and at 1x, 1.25x and 2x. A check with the old two rails
  fails.
- AC2: Accept reads as a button and the File and Changes captions stay plain
  labels: Accept carries an icon beside its word inside the strip's button
  frame, the only text button in that stretch of the bar; the captions keep
  no frame. In the accepted state it reads **Revoke acceptance** in the
  checked frame; while it is disabled (no comparison, ledger unavailable,
  excluded file) the frame and icon take the disabled ink. Light and Dark.
- AC3: Behaviour is unchanged: the one retained Accept action, one click
  without a menu, Option-A (Alt+A) with its tooltip, the accessible name and
  description, the focus order, and the bar's width floor and yields.
- AC4 (human): The owner accepts the frozen candidate's review bar, before
  and after, in Light and Dark, unaccepted, accepted (Revoke) and disabled.

## Coordination

Stay within the review bar's Accept and private-progress group. Other cards
are restyling the Overview toggle (BPR-1577), adding refresh and loading
states (BPR-1575, BPR-1576) and redesigning the drawers, Agent and Compose
(BPR-1574). Load `frontend-design:frontend-design` before UX work. Never drive
or capture the owner's desktop; the owner looks at the frozen build.
