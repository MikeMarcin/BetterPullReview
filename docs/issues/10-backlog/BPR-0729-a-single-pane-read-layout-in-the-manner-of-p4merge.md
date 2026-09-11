---
schema: "implement-rfc.card.v1"
id: "BPR-0729"
title: "A single-pane Read layout in the manner of p4merge"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"205ac8b2-30fd-4339-8d08-51c621ab7fc8"}
---

# A single-pane Read layout in the manner of p4merge

The owner chose, on the options candidate `BPR-0724-read-width` in the
v006 attended session (decision `205ac8b2`, 18:42:22Z), "A single-pane
Read layout (one document with the other side's changes marked) as a new
slice", and wrote: "Toggle to single pane view like p4merge's layout would
be good". Their two screenshots of p4merge
([1](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png),
[2](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-2-2026-09-11.png))
show the shape: one document in reading order; removed lines kept in place,
struck through, on a green field; added lines in place on a purple field;
unchanged text on the plain background; a change map in the left gutter;
a toolbar toggle between the two-pane and one-pane presentations.

## Why this is a new slice

RFC-BPR-0028's Read is two whole-document panes over the DiffLinkMap
(`diff_workbench.cpp`, `Comparison->setSizes({500, 500})`), and the diff
colour themes and the link map are settled work the RFC says are rendered
as they ship. A one-pane presentation is a new diff surface: it needs a
row model that interleaves both sides in one column (the core already
computes the aligned groups; the surface is presentation over that
result), its own thread alignment for Read's comment cards (AC-05), a
toggle in the strip or the Read preset, per-window persistence of the
choice, and the diff themes applied to full-row fields rather than to the
two panes. The RFC is amended before the slice starts: a design round on
the accepted asset (a one-pane state beside the two-pane one), then the
Modalities paragraph and AC-05.

## Scope, to be fixed by the design round

- A toggle between two-pane and one-pane Read, persisted per window.
- The one-pane row model over the existing computed diff: removed rows
  struck through in place, added rows in place, each on its theme field;
  the change map in the gutter; navigation (next/previous change) and
  line identity unchanged.
- Read's aligned comment cards follow the rows.
- Owned paths: `src/adapters/qt/src/diff_workbench.cpp` and a new one-pane
  surface beside it, the workbench window's Read preset and strip, their
  tests and render evidence; the core's diff result is not changed.

## Acceptance, to be fixed by the design round

- The one-pane surface shows the same lines the two panes show, in one
  column, with displayed line identity verified against the expected data
  on a representative fixture.
- The toggle, its persistence and the comment alignment are pinned in
  `qt_workbench_window` and `qt_composed_github`.
- The owner accepts the layout on the signed bundle in an attended
  session.
