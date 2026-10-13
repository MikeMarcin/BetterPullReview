---
schema: "implement-rfc.card.v1"
id: "BPR-1471"
title: "Discuss's new-post counts share one column"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-1471--BPR-1471--f569dda6851d.json","origin":"owner request 2026-10-13"}
acceptance: []
---

# Discuss's new-post counts share one column

The owner reported on 2026-10-13: "the blue circles dont always line up on
the discuss tab depending on if the green thing is there or not. It looks
ugly when they arent all aligned."

On Discuss, each closed note's meta row is laid out from the right edge. A
resolved note draws the green resolved check there first. The blue new-post
count and everything to its left (the reply count and the repliers' faces)
then start 20 px further left than they do on an unresolved note, so the
blue counts of a mixed list do not line up.

## Acceptance

- AC1: On every closed note that is not a draft, the blue new-post count
  stands at the same horizontal position whether or not the note carries the
  green resolved check. The reply count and the repliers' faces do the same.
- AC2: A Qt test asserts that geometry across resolved and unresolved notes.
- AC3: Before and after offscreen renders of the Discuss notes exist in the
  build tree. They are not committed.
