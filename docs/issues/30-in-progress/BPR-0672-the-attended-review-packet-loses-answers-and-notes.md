---
schema: "implement-rfc.card.v1"
id: "BPR-0672"
title: "The attended review packet loses answers and notes"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
---


# The attended review packet loses answers and notes

The owner reviewed the RFC-BPR-0028 attended session on 2026-09-07 and could
not complete it. In their words: "I have no way to reset/clear an answer if I
accidentally did one", "It looks like it lost my response note when I clicked
reject on the first one", "There's no place to add images so I have to use the
fallback flow", and "The fallback flow didn't have the decision I made in the
json export. Literally unusable."

Four defects, and the fourth is the serious one: a decision the reviewer made
was not in the export, so the record of what they decided was lost. The session
is the only durable record of an owner decision; losing one is worse than
never taking it.

## Scope

- Reproduce each of the four against the shipped page before changing it: a
  recorded decision that cannot be changed or cleared, a note discarded when
  the verdict is recorded, no way to attach an image to a verdict, and an
  offline export missing a decision the page had shown as saved.
- A verdict is editable until it is imported, and clearing one is offered.
- The note is part of the verdict and is saved with it.
- A verdict carries images, because a UI rejection is usually a picture.
- The export contains every decision the page shows as saved, and the page
  cannot show a decision as saved that the export would omit.
- The packet launches what it asks the reviewer to look at. The owner had to
  navigate a Finder tree to a path in prose: "the packets instructions to
  launch the app had me manually navigating my finder tree instead of a simple
  command or click". A prepared candidate that names a built application offers
  a one-click open or a copyable command.
- The skill is shared, so the fix lands in both copies and their equality is
  proved.
- Owned paths: the attended-review skill's script and page under
  `.claude/skills/attended-review` and `.agents/skills/attended-review`, and
  its tests.

## Acceptance

- Each of the four is reproduced, fixed, and covered by a test that fails
  before the fix.
- A session run end to end records every verdict, note and image, and the
  export and the server intake agree.
- Nothing in a packet asks the reviewer to find a file by hand.
