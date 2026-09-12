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
