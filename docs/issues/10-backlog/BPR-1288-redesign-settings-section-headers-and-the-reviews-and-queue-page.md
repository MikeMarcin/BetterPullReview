---
schema: "implement-rfc.card.v1"
id: "BPR-1288"
title: "Redesign the Settings section headers and the Reviews & queue page"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner asides in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 appearance-tints-v001, decision 74a8eac0; mark-style-v001, decision b504ffed; 2026-10-06)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1088","BPR-1289"],"owner_references":["docs/design/owner-refs/BPR-1083-owner-ref-appearance-section-headers.png","docs/design/owner-refs/BPR-1083-owner-ref-reviews-and-queue.png"],"implementation_note":"Use the frontend-design skill when implementing; owner review is required","acceptance":["BPR-1288:headers","BPR-1288:reviews-and-queue","BPR-1288:kept","BPR-1288:themes","BPR-1288:owner-look"]}
---

# BPR-1288: Redesign the Settings section headers and the Reviews & queue page

## Origin

Accepting `BPR-1083-appearance-tints-v001`, the owner added, quoted exactly:

> Aside: These section labels in settings are super ugly, needs some frontend
> design love.

Their screenshot, [`BPR-1083-owner-ref-appearance-section-headers.png`](../../design/owner-refs/BPR-1083-owner-ref-appearance-section-headers.png),
shows Appearance in the dark theme: "Colours for Dark" (with Import…,
Export… and Reset all) and "Dif" (with Reset) as flat, tall, mid-grey bars
that span the page.

Accepting `BPR-1083-mark-style-v001`, they added, quoted exactly:

> This reviews and queues settings page is incredibly ugly and needs some
> frontend design love.

Their screenshot, [`BPR-1083-owner-ref-reviews-and-queue.png`](../../design/owner-refs/BPR-1083-owner-ref-reviews-and-queue.png),
shows Reviews & queue: group names (Modality rules, Reading, Drawers, Comment
labels) set like the rows under them, a Reset per group pushed to the far
edge, segmented rows of square buttons, and a boxed Queue defaults form with
right-aligned labels and a full-width Reset queue defaults bar.

## Outcome

Settings sections read as sections on every page: one header treatment, with
the section's actions placed with it, used by every page. Reviews & queue in
particular gets a considered layout: clear groups, each setting's name and
what it does, controls aligned on one column, and the queue defaults in the
same language as the rest.

Use the `frontend-design` skill when implementing. Prepare the design as
frames first if the direction is not obvious, and ask the owner one Accept or
Reject per candidate.

## Acceptance

- **BPR-1288:headers.** One section header is used on every Settings page;
  no page keeps the flat grey bars. Section actions (Import, Export, Reset
  and the rest) stay with their section and keep their names.
- **BPR-1288:reviews-and-queue.** Reviews & queue is regrouped and aligned
  as the accepted design says, including Queue defaults and the Label marks
  row with its preview (BPR-1083).
- **BPR-1288:kept.** Every setting still reads, writes, resets and persists
  exactly as before; object names and accessible names that tests and screen
  readers use stay or are migrated with their tests; keyboard order follows
  the visual order.
- **BPR-1288:themes.** Light, dark and both high-contrast variants, larger
  text and narrow windows render without clipping; colours come from the
  palette, not fixed values.
- **BPR-1288:owner-look.** The owner accepts the result in an attended
  session. Required, not self-certified.

## Boundaries

Presentation only, in the Qt adapter. BPR-1088 owns the Comment labels
helper row and BPR-1289 the Settings search; whichever lands second adopts
the other's layout.
