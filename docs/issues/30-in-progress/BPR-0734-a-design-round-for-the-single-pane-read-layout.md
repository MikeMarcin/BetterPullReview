---
schema: "implement-rfc.card.v1"
id: "BPR-0734"
title: "A design round for the single-pane Read layout"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","sequenced_after":"rfc-bpr-0028-attended-v007 verdicts"}
---

# A design round for the single-pane Read layout

[BPR-0729](../10-backlog/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md)
is a new diff surface and RFC-BPR-0028 says it is amended before the slice
starts: a design round on the accepted asset, then the Modalities paragraph
and AC-05. Asked on 2026-09-11 whether that round should start now or after
the v007 verdicts, the owner answered: "Card it, can happen after the
verdict". This card is the round. It is sequenced after the owner's v007
verdicts on the four parked candidates (BPR-0646, BPR-0663, BPR-0676,
BPR-0720) and is not a dependency of any of them; BPR-0729 depends on it.

## Scope

- One new state on the accepted asset
  ([workbench states v5](../../design/RFC-BPR-0028-workbench-direction-v5.html)
  or a v6 beside it): Read in one pane, in the shape of the owner's two
  p4merge screenshots
  ([1](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png),
  [2](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-2-2026-09-11.png)):
  one document in reading order; removed lines kept in place, struck
  through, on their theme field; added lines in place on theirs; unchanged
  text plain; the change map in the gutter; the toggle between two-pane and
  one-pane in the strip or the Read preset. The diff colour themes are
  settled and off-limits ([RFC-BPR-0028 non-goals](../../rfcs/RFC-BPR-0028-review-workbench-direction.md));
  the round applies them to full-row fields, it does not redesign them.
- Where Read's aligned comment cards sit against one column (AC-05), and
  what next/previous change and line identity mean in one column.
- The amendment text for the Modalities paragraph and AC-05, drafted for
  the owner with the state, so BPR-0729's scope and acceptance can be
  fixed on the card the day the round is accepted.
- Owned paths: `docs/design/` (a new asset version, never an edit to an
  accepted one), RFC-BPR-0028, BPR-0729's card. No product source.

## Acceptance

- The owner accepts the one-pane state and the two amendments in an
  attended session, or in the round's own review page; the decision is
  recorded on this card and on RFC-BPR-0028.
- BPR-0729's "to be fixed by the design round" scope and acceptance are
  rewritten from the accepted state before BPR-0729 is claimed.

## Round output, 2026-09-11

The round's state is
[workbench states v6, "Read, one pane"](../../design/RFC-BPR-0028-workbench-direction-v6.html#read-one-pane)
(state 2b, anchor `read-one-pane`). v6 is a copy of the accepted v5 with
that one state added beside Read. v5's other states, its diff colours and
its CSS variables are unchanged; the new state applies the shipped
`--add`, `--del`, `--add2` and `--del2` fields to whole rows and invents
no colour.

### Proposed amendment, the Modalities paragraph

To follow "and keeps comments beside the code;" in RFC-BPR-0028's
Modalities paragraph, as proposed text:

> Read presents the comparison in one of two ways: two whole-document
> panes over the link map, or one document in reading order with the
> other side's lines kept in place and struck through on their theme
> field. The presentation is chosen from the strip or from the Read
> preset, it is remembered per window alongside the modality, and it
> changes nothing about which lines the comparison contains
> ([[BPR-0729]], owner direction 2026-09-11).

### Proposed amendment, AC-05

Replacing AC-05 whole, as proposed text:

> - AC-05 Read shows this file's threads aligned to their lines with
>   unread state and drafts, in both presentations; in one pane a thread
>   names a side and a line and sits against the row carrying that
>   identity, so a thread on a removed line sits against its struck row
>   and a thread on an added line against the added row, and threads
>   that would collide keep document order and are pushed down with
>   their leader still on the anchored row; Discuss shows the list, map
>   and feed with the listed filters.

### What the state decides

- Line identity. Every row keeps the side and the line number it came
  from, in two number columns, base and head. An unchanged row carries
  both numbers; a struck row carries only its base number and is the old
  side's identity; an added row carries only its head number. Comment
  anchoring, quoting and Open in code use the row's own side and number,
  so nothing written against a removed line is re-attributed to the new
  file.
- Next and previous change. A change is one block: its struck rows
  followed by its added rows. `‹` and `›` step between blocks, not rows,
  and the ordinal is the same number in both presentations, so "change 3
  of 11" names the same change in either and switching presentation does
  not move the reader. Stepping lands on the block's first row, which is
  the struck row whenever the change removes anything.
- Unchanged text is plain: no field, no strike. Only change blocks carry
  a field, and the field covers the whole row, number columns included.
- The gutter. One column has nothing to link across, so the strip the
  link map held between the panes becomes a change map at the left: the
  whole file's changes as ticks coloured by kind, the visible window as
  a lens, and the on-screen changes as bands beside the rows they mark.
  The two-pane state's link map is untouched.
- The toggle is two icons in the strip beside the change stepper, on the
  Read preset, remembered per window with the modality.

### Open questions for the owner

1. Block order. The state puts the struck old rows above the new rows,
   the unified-diff order every review tool here uses. The p4merge
   captures put the kept side's block first and the struck block below
   it. The state flips in one line if the owner prefers p4merge's order.
2. Where the choice lives. The state makes one pane a property of Read,
   remembered per window, so leaving for Review and coming back returns
   to one pane while Review keeps two panes. The alternative is a
   property of the window that Review inherits too.
3. The gutter map's standing. RFC-BPR-0028's non-goals rule out changes
   to the link map. The one-pane gutter map is a new element rather than
   a changed one; if the non-goal is to stay literal, the Modalities
   amendment should name it as new.
