---
schema: "implement-rfc.card.v1"
id: "BPR-2180"
title: "Other panes load with the shape of their content"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-11-14","owner_note":"Hey can we do more graphics like the main pane loading for other parts that are loading in?","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-1576","BPR-1842","BPR-2151"],"reservation":"docs/issues/_reservations/BPR-2180--BPR-2180--cfc3f23d5086.json"}
---

# BPR-2180: Other panes load with the shape of their content

The owner asked on 2026-11-14: "Hey can we do more graphics like the main
pane loading for other parts that are loading in?" Their screenshot showed the
comparison pane's code skeleton beside a Comments drawer that said its wait in
words alone.

[[BPR-1576]] made one loading-state pattern, `qt::pending_veil`: placeholders
in the content's own shape, then the turning mark and words. The comparison
pane draws code; the Comments drawer and Threads draw plain bars; the
Overview's people, the private tour and Finish review's pending comments say
"Loading…" in text only, and the status bar says "Preparing file
comparisons…" without the mark the Updates line has.

Governing design: the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and the
loading-state pattern recorded in the
[workbench specification](../../ux/workbench-specification.md). This extends
the accepted pattern; it adds no shimmer and no new motion.

## Acceptance

- AC01: The pattern gains a `cards` shape (a thread card: feedback rail, face,
  name and age, two lines of text) and a `people` shape (faces in a row where
  the place is one line high, a roster of face and name where it is taller),
  drawn from the same tint, theme, high-contrast and reduced-motion rules as
  the other shapes. A pane whose wait a heading beside it already says can
  draw its placeholders in the words' place.
- AC02: The Comments drawer (and Discuss, which shows the same panel) and
  Threads wait as thread cards.
- AC03: The Overview's People section waits as faces while reviewers are still
  coming, after the title and files have shown.
- AC04: The private tour waits as stop cards under its head while it loads.
- AC05: Finish review holds a place for its pending comments while they are
  read, with comment rows under its "Loading pending comments…" heading.
- AC06: The status bar's line turns the pattern's mark while it says the pull
  request or its comparisons are loading.
- AC07: Qt and composed courses cover AC01 to AC06, with renders in Light and
  Dark; format, tidy and documentation gates pass.
- AC08 (human): The owner accepts a frozen build opening a slow pull request,
  one Accept/Reject per surface group.

The Agent tab's load is deferred: its view belongs to [[BPR-2151]] while that
card is open.
