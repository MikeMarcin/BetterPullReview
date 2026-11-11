---
schema: "implement-rfc.card.v1"
id: "BPR-2093"
title: "Overview's Comments column shows each file's drafts"
status: "In Progress"
rfcs: ["RFC-BPR-0085", "RFC-BPR-0049"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1089","BPR-2092","BPR-2094"]}
depends_on: []
---

# Overview's Comments column shows each file's drafts

## Origin

The owner, 2026-11-11: "Actually it's fine is do stuff for drafts on overview
as well", "I would put in next to the file on overview", "Yeah I mean we
already have a comments column so probably something there".

## Scope and acceptance

1. A file row's Comments cell shows the file's unsent drafts as one chip
   beside its comment chips: a draft count, with a distinct mark when any is
   not in the review. It matches the column's chips in style and keeps the
   column's fixed width; a full cell folds comment chips into "+n" before the
   drafts chip. A folder row adds up its files.
2. The chip's tooltip and accessible text say how many drafts are in the
   review and how many are not.
3. A click (or the keyboard's activation) on the chip goes to that file's
   drafts: Threads with My drafts on and the file's first draft under the
   cursor.
4. The counts are the core's (BPR-2092's standing and place).
5. Owner look: Accept or Reject the column.

Load frontend-design before the Qt work.
