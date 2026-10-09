---
schema: "implement-rfc.card.v1"
id: "BPR-1288"
title: "Redesign the Settings section headers, info glyphs, and the Reviews & queue and Agents & context pages"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner asides in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 appearance-tints-v001, decision 74a8eac0; mark-style-v001, decision b504ffed; 2026-10-06); owner asides accepting BPR-1090-feedback-row-v001 in attended session df50c8413c8a1b3a29db57916d6b8941 (decision 51eb7b84, 2026-10-09T02:28Z)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1088","BPR-1289","BPR-1090"],"owner_references":["docs/design/owner-refs/BPR-1083-owner-ref-appearance-section-headers.png","docs/design/owner-refs/BPR-1083-owner-ref-reviews-and-queue.png","docs/design/owner-refs/BPR-1090-owner-ref-section-banner.png","docs/design/owner-refs/BPR-1090-owner-ref-info-buttons.png","docs/design/owner-refs/BPR-1090-owner-ref-agents-page.png"],"implementation_note":"Use the frontend-design skill when implementing; owner review is required","acceptance":["BPR-1288:headers","BPR-1288:reviews-and-queue","BPR-1288:agents-and-context","BPR-1288:info-glyphs","BPR-1288:words","BPR-1288:kept","BPR-1288:themes","BPR-1288:owner-look"]}
---

# BPR-1288: Redesign the Settings section headers, info glyphs, and the Reviews & queue and Agents & context pages

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

### Owner input, 2026-10-09: asides accepting BPR-1090's Review feedback row

Accepting `BPR-1090-feedback-row-v001` (session
`df50c8413c8a1b3a29db57916d6b8941`, decision `51eb7b84`, 02:28Z), the owner
wrote in the note, quoted exactly (line breaks as given):

> aside: these gray section banners with white text are ugly as sin
> aside 2: something looks off about all these little info buttons, i dont tink theyre supposed to have that rect, and its not centered if they are.
>
> There's some layout problems on this agents page and a crapton of text.

The three screenshots they attached, from the frozen BPR-1090 build in the
dark theme:

- [`BPR-1090-owner-ref-section-banner.png`](../../design/owner-refs/BPR-1090-owner-ref-section-banner.png):
  Appearance's "Theme" section header, a tall mid-grey bar spanning the page
  with white "Theme" and "Reset". The first aside restates
  `BPR-1288:headers`.
- [`BPR-1090-owner-ref-info-buttons.png`](../../design/owner-refs/BPR-1090-owner-ref-info-buttons.png):
  Appearance › Type, each setting name followed by its information glyph
  (`info_glyph`) drawn inside a raised square button frame, with the circled
  "i" visibly off centre in it.
- [`BPR-1090-owner-ref-agents-page.png`](../../design/owner-refs/BPR-1090-owner-ref-agents-page.png):
  Agents & context › Claude. Right-aligned labels (Process wrapper, Sign-in)
  in a form whose explanatory paragraphs are cut off at the top and bottom of
  fixed-height areas, the Sign-in explanation is clipped through its first
  line, and the status lines, the wrapper explanation and the sign-in note
  are stacked as long paragraphs.

The section banners were already this card's scope. The info glyphs, the
Agents & context layout and its amount of text were not, and are added
below (`BPR-1288:info-glyphs`, `BPR-1288:agents-and-context`,
`BPR-1288:words`).

## Outcome

Settings sections read as sections on every page: one header treatment, with
the section's actions placed with it, used by every page. Reviews & queue in
particular gets a considered layout: clear groups, each setting's name and
what it does, controls aligned on one column, and the queue defaults in the
same language as the rest. Agents & context gets the same care: no clipped
or overlapping text at any size, labels and controls on one alignment, and
far fewer words, with the explanation that remains reachable from an
information glyph or tooltip rather than standing paragraphs. The
information glyph is a bare, centred mark wherever Settings uses it.

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
- **BPR-1288:agents-and-context.** Agents & context (Claude, AI assistance
  and the writing hooks) is laid out as the accepted design says. No text is
  clipped, cut through or overlapped at any supported size, scale or window
  width; each explanation shows whole or is reached on demand.
- **BPR-1288:info-glyphs.** The information glyph (`info_glyph`) draws no
  button rectangle or frame in any state, and its mark is centred, on every
  Settings page that uses it. Its consequence text, tooltip, accessible name
  and keyboard reach stay.
- **BPR-1288:words.** Settings text is cut to what a reviewer needs to choose:
  each row has a short name and at most one short line, longer explanations
  move into the glyph or tooltip, and the card records the before and after
  word counts for Agents & context and Reviews & queue.
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
