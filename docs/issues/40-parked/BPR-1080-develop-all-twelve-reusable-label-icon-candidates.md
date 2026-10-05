---
schema: "implement-rfc.card.v1"
id: "BPR-1080"
title: "Develop all twelve reusable label icon candidates"
status: "Parked"
rfcs: ["RFC-BPR-0050"]
depends_on: ["BPR-1079"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-06","milestone":"design","acceptance":["R30","R31"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1080:roles","outcome":"pass","revision":"44d09faa3","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-frames-v4.html","detail":"Round v4: twelve distinct glyphs, one per catalog ID, no alias or omission; eleven are v3's byte for byte (the accepted set, including the broom accepted in session bfc59159). ?check=1 on the frozen source, 16 dumps: min cross-label separation 0.571 at 12 px, 0.483 at 24 px (issue/note, unchanged); typo 0.73/0.74 from its nearest neighbour (todo), 0.776/0.80 from quibble."}},{"criterion":"BPR-1080:previews","outcome":"pass","revision":"44d09faa3","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-v4.html","detail":"Actual 16 px marks normal and blocking in light, dark, high-contrast light and dark, 2.5x, greyscale and grid; v3 beside v4 with the owner's Abc reference; thread cards and surfaces; rendered at 1x and 2x from the frozen source; 1x pixels of the redrawn typo inspected in four variants, normal and blocking. Names unchanged; glyph contrast >= 6.06 soft, >= 4.52 blocking. Static frames, not native evidence (R33)."}},{"criterion":"BPR-1080:provenance","outcome":"pass","revision":"44d09faa3","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-v4.html","detail":"Coordinates hand-written by the rfc-implementer; the owner's three reference images (docs/design/BPR-1080-owner-ref-*.png) set the style; nothing traced. QPainterPath data (M/L/C/Z, discs, filled paths with opposite-wound holes, per-element pen width) compiled into the Qt adapter; no SVG, Qt SVG, icon font, runtime fetch or build-time generation. Typo not approved."}},{"criterion":"BPR-1080:owner-selection","outcome":"pending","revision":"44d09faa3","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0047-0051.md","detail":"Round v3 verdicts recorded (session bfc59159: chore accepted; typo rejected). Eleven glyphs now owner-accepted. Round v4 pending in attended session d7393d7131b629b535e95f60cfb9564b (state /Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v004, ready at sequence 3): Accept/Reject on typo-v004, on frozen root /Users/jaeger/bpr1080-label-icons-review-v004.1AmFcn."}}]
---

# BPR-1080: Develop all twelve reusable label icon candidates

Pack alias: **PACK-CC-06** (live `BPR-1080`, allocated at import).

## Outcome and work

Produce a complete icon-role candidate set in the existing visual language, with source provenance and actual-size previews. Evaluate related labels together and color-independent legibility. Decide the resource pipeline without an icon-font dependency or runtime asset retrieval. Dif art is outside this task.

## Acceptance

- All twelve IDs have candidate roles; none is silently aliased or omitted.
- Small-size, theme and scale previews include accessible text/name strategy.
- Provenance and asset-source ownership are recorded; no candidate is asserted approved by this pack.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Candidates, round v1 (2026-10-05)

[Record](../../design/BPR-1080-label-icons-v1.html) and
[frames](../../design/BPR-1080-label-icons-frames-v1.html), at candidate
`c93008559` (on main as `aff980294`, with byte-identical files). The icons replace the two letters inside the marks that BPR-1079
selected (the 16 px tinted square, filled when blocking, under the avatar with
the rail). Tints are unchanged.

- **Twelve roles, with no aliases.** praise heart, nitpick magnifier on a
  speck, suggestion light bulb, issue exclamation mark, todo ticked box,
  question question mark, thought thought cloud, chore wrench, note
  information i, typo letter over a spelling squiggle, polish sparkles, quibble
  swap arrows. Only three labels have real alternatives: praise (thumbs up,
  star), chore (broom) and quibble (plus-minus). Tweezers for nitpick and
  scales for quibble were tried and dropped, because they were illegible at 1×.
- **Frames.** A sheet shows all twelve, normal and blocking, in light, dark,
  high-contrast light and high-contrast dark, then at 2.5×, in greyscale and on
  the 24-unit grid. A second frame shows the alternatives. Others show the
  Overview chips, the ⌘L menu, completion and the Label key, and thread cards at
  1440×820 and 960×760, light and dark, reading and editing, with every label
  on a post.
- **Text and accessibility.** Each mark is an image named "issue (blocking)" and
  so on, with the same words in its tooltip. The keyboard-reachable Label key
  lists name and shortcode. Shape carries the label and the tint only adds to
  it. The public prefix stays the source.
- **Resource form.** The glyphs are path data on the app's stroked-glyph grid
  (24 units, round pen, as `workbench_strip.cpp` draws). The data uses only
  M/L/C/Z plus discs, so it maps one-to-one onto `QPainterPath` and compiles
  into the Qt adapter. There are no SVG files, Qt SVG module, fonts, runtime
  fetches or generated images.
- **Provenance.** The `rfc-implementer` agent (Claude Opus 5.5) wrote every
  coordinate by hand for this card. No icon set, tracing or image generation
  was used, and nothing comes from a chat sketch. The data is the
  repository's.
- **Measured.** These come from `?check=1` on the frozen source, 16 renders.
  - Pairwise ink-overlap separation across labels is at least 0.54 at 12 px
    and 0.48 at 24 px (issue and note).
  - Suggestion and polish separate at 0.79, and nitpick and quibble at 0.69
    with swap and 0.86 with plus-minus.
  - Glyph contrast is at least 6.06:1 on the soft fill and 4.52:1 on the
    blocking fill, across four theme variants.
  - Mark placement is unchanged from v4: 0 px offset, 3 px gap, rail 3 px.
    Names match.

Limitations: these are static HTML frames rendered with headless Chrome. They
are not native, scaled, high-contrast or assistive-technology evidence (R33).
Nothing is implemented, and no candidate is approved until the owner selects
it (R30).

Owner review: session `82eb35d69600801ab5f8875d88d836a2` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v001`,
ready at sequence 12). It asks three option questions, for praise, chore and
quibble, and one Accept/Reject for the other nine. The frozen root is
`/Users/jaeger/bpr1080-label-icons-review-v001.BP1nCI`.

## Owner verdicts on round v1 (2026-10-05)

Session `82eb35d69600801ab5f8875d88d836a2` was handed back at sequence 26, with
nothing pending. `status --after 12` was read, and the host had already been
stopped. Final verdicts, verbatim:

- `BPR-1080-praise-v001`: **selected A: heart**, with no note.
- `BPR-1080-chore-v001`: **Reject**. "Broom is good but that broom is ugly."
  Attached: [broom reference](../../design/BPR-1080-owner-ref-chore-broom.png)
  (`image.png`, 274×324, SHA-256 `0bebc3b7…46858e`). It shows a solid broom
  with a diagonal handle, a band and a fanned bristle head with splits.
- `BPR-1080-quibble-v001`: **Reject**, with no note. Attached: ["small wobble"
  reference](../../design/BPR-1080-owner-ref-quibble-small-wobble.png)
  (`image.png`, 250×328, SHA-256 `c2753d57…6e5536`). It shows a wavy,
  tilde-like line with three short rays above it.
- `BPR-1080-set-v001` (the other nine): **Reject**. "nitpick make a better
  loupe, chore make a broom, type do spellcheck aka Abc over squiggle, quibble
  do attached". Attached: the same small-wobble image (byte-identical).

Earlier decisions in the same session were superseded before handback. Chore
was first selected as B (broom) at sequence 13, then rejected. Praise was
rejected at sequences 14, 15 and 17, with the note "Avoid things that are
already github emoji reactions, and stars on github already has a meaning
too. Maybe more like laurel sprigs?", then selected as A (heart) at 24. Only
the final verdicts bind. The two reference images are committed as owner
references; they contain no secrets.

## Candidates, round v2 (2026-10-05)

[Record](../../design/BPR-1080-label-icons-v2.html) and
[frames](../../design/BPR-1080-label-icons-frames-v2.html). Round v1's files
are unchanged.

- **Kept.** Praise keeps the selected heart. Suggestion, issue, todo,
  question, thought, note and polish are v1's glyphs, unchanged.
- **Redrawn.**
  - nitpick: a loupe with an open ring and a thick, round-ended handle; the
    speck inside the lens is gone.
  - chore: a solid broom in the reference's style, with a diagonal handle, a
    band and a fanned head split twice.
  - typo: spellcheck, "Abc" over a zigzag squiggle. The letters use a 2-unit
    pen (1 px at 1×) on the 1× pixel lattice, so all three stay distinct at
    16 px.
  - quibble: the small wobble, one hump with flicked-up ends and three rays.
- **Resource form.** It is still `QPainterPath` data and M/L/C/Z plus discs.
  Two element kinds are new: a filled path (`fillPath`, no pen) and a stroke
  with its own pen width (typo's 2.0). There is still no SVG, font or runtime
  asset.
- **Provenance.** The `rfc-implementer` wrote the coordinates by hand for this
  card. The owner's images set the style; nothing was traced from them (R30).
- **Measured** with `?check=1` over 16 dumps:
  - Minimum cross-label separation is 0.57 at 12 px and 0.48 at 24 px. That
    is issue and note, unchanged from v1.
  - Every redrawn glyph is at least 0.66 from its nearest neighbour. Typo and
    quibble measure 0.81.
  - Contrast is unchanged: at least 6.06:1 soft and 4.52:1 blocking, in four
    theme variants.
  - Placement and names are unchanged.

Limitations: these are static frames rendered with headless Chrome. They are
not native evidence (R33), and nothing is approved until the owner accepts it.

Owner review: session `22be5050669115b022366b9e8e56a6c5` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v002`,
ready at sequence 15). It asks five Accept/Reject questions, one for each of
nitpick, chore, typo and quibble and one for the twelve together. The frozen
root is `/Users/jaeger/bpr1080-label-icons-review-v002.rEBx7h`. The v001 root
was deleted after its decided candidates were released.

## Owner verdicts on round v2 (2026-10-05)

Session `22be5050669115b022366b9e8e56a6c5` was handed back at sequence 21, with
nothing pending. The host was already stopped. `status --after 15` returned
decisions at sequences 16–20 and `handed-back` at 21. Verdicts, verbatim:

- `BPR-1080-nitpick-v002`: **Accept**, with no note.
- `BPR-1080-quibble-v002`: **Accept**, with no note.
- `BPR-1080-set-v002` (all twelve together): **Accept**, with no note.
- `BPR-1080-chore-v002`: **Reject**. "Make the broom look better."
- `BPR-1080-typo-v002`: **Reject**. "I dont like the A". Attached:
  [Abc reference](../../design/BPR-1080-owner-ref-typo-abc.png) (`image.png`,
  266×324, SHA-256 `7a9deb0b…c5d9`). It shows a bold geometric sans "Abc" over
  a smooth sine squiggle, all one weight.

The loupe (nitpick), the small wobble (quibble) and the set are accepted as v2
drew them. Chore and typo stay open.

## Candidates, round v3 (2026-10-05)

[Record](../../design/BPR-1080-label-icons-v3.html) and
[frames](../../design/BPR-1080-label-icons-frames-v3.html). The files from
rounds v1 and v2 are unchanged.

- **Kept.** The other ten glyphs are v2's, byte for byte. A script compared
  the two `GLYPHS` tables entry by entry, and the tints are identical.
- **Redrawn.**
  - chore: a broom closer to the owner's reference. It has a thinner handle
    (a 2.2-unit stroke), a band, a clear seam, and a broad head that fans out
    and is split twice from the foot. It is drawn at 45°, with every edge on
    an odd value of x − y or x + y. Those values fall between the pixel
    diagonals at 1×, so the seam and the splits render as clean diagonals.
  - typo: "Abc" in the reference's style. The A is a single filled shape with
    straight legs, a flat apex on the centre pixel, a flat crossbar and a
    counter, all symmetric. The b and c are round 2-unit strokes on the 1×
    lattice. The squiggle is a smooth sine of 3.5 periods. All of it carries
    one weight.
  - Only the best variant of each is presented. Also tried for chore: a
    free-angle broom with fanned splits, which was spotty at 1×, and lattice
    brooms with other section widths. Also tried for typo: wider, bolder
    free-drawn letters, which ran together at 1×, and narrower letters.
- **Resource form.** Unchanged. The A's hole winds against its outline, so
  the nonzero and odd-even fill rules agree.
- **Provenance.** The `rfc-implementer` wrote the coordinates by hand. The
  broom's angle and proportions follow the owner's reference. Nothing was
  traced (R30).
- **Measured.** `?check=1`, 16 dumps:
  - The minimum cross-label separation is still issue and note, at 0.57
    (12 px) and 0.48 (24 px).
  - Each redrawn glyph against its nearest neighbour (todo in both cases):
    chore 0.65 and 0.61, typo 0.76 and 0.76.
  - Typo and quibble separate at 0.79 and 0.80.
  - Contrast is at least 6.06:1 on the soft fill and 4.52:1 on the blocking
    fill. Placement and names are unchanged.
  - The 1× renders of both marks were inspected pixel by pixel in all four
    variants.

Limitations: static headless-Chrome frames, not native evidence (R33). Chore
and typo are not approved until the owner accepts them.

Owner review: session `bfc59159786ef55286d1c1613b87f595` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v003`,
ready at sequence 6). It asks two Accept/Reject questions, `chore-v003` and
`typo-v003`. Each shows v2, v3 and the owner's reference, plus the 1× marks
enlarged. The frozen root is `/Users/jaeger/bpr1080-label-icons-review-v003.WtBVGN`.
The v002 session's candidates were released at its sequence 22, with the
verdicts kept, and its frozen root was deleted.

## Owner verdicts on round v3 (2026-10-05)

Session `bfc59159786ef55286d1c1613b87f595` was handed back at sequence 9, with
nothing pending. The host was already stopped. `status --after 6` returned
decisions at sequences 7 and 8 and `handed-back` at 9. Verdicts, verbatim:

- `BPR-1080-chore-v003`: **Accept**, with no note.
- `BPR-1080-typo-v003`: **Reject**. "I think you can do a little better, that
  b looks far from the A and that c looks cut off"

The v3 broom is accepted for chore. Typo stays open.

## Candidates, round v4 (2026-10-05)

[Record](../../design/BPR-1080-label-icons-v4.html) and
[frames](../../design/BPR-1080-label-icons-frames-v4.html). The files from
rounds v1 to v3 are unchanged.

- **Kept.** The other eleven glyphs are v3's, byte for byte, including the
  accepted broom. A script compared the two `GLYPHS` tables entry by entry,
  and the tints are identical.
- **Redrawn: typo**, "Abc" set as one word over v3's sine squiggle.
  - Why v3's c looked cut off: the 1× glyph box is 12 px, and a 5 px A, a
    3 px b and a 3 px c with a clear pixel between each need 13. v3 kept both
    gaps and the full A, so the c got 2 px and ran into the border.
  - The A keeps its bold, symmetric shape and its flat apex on the centre
    pixel. Its feet move half a pixel in on each side (0.5 and 9.5), its legs
    are heavier (2.4 units across), and its crossbar sits a pixel lower
    (10–12), which opens a counter at 1× and at 2×.
  - The b's stem follows the A at once (10–12). The gap closes towards the
    baseline, as in the owner's reference. The bowl keeps a one-pixel
    counter, and a clear pixel column parts b from c.
  - The c is three pixels wide, square-cut at 23.5, a whole and open c that
    stops a pixel short of the border.
  - All three letters are filled shapes. The squiggle is unchanged.
  - Only the best variant is presented. Also tried: v3's A with the b kerned
    onto its foot, which merged at 1×; a narrower A, which lost weight; the
    word over the full 24 units, which put the b on half pixels; a c reaching
    the box edge, which looked clipped again; heavier b and c strokes, which
    greyed the b's counter; and other crossbar heights.
- **Resource form.** Unchanged in kind. The A's and b's holes wind against
  their outlines, so the nonzero and odd-even fill rules agree.
- **Provenance.** The `rfc-implementer` wrote the coordinates by hand in the
  style of the owner's Abc reference. Nothing was traced (R30).
- **Measured.** `?check=1`, 16 dumps:
  - The minimum cross-label separation is still issue and note, at 0.57
    (12 px) and 0.48 (24 px).
  - Typo against its nearest neighbour (todo) is 0.73 and 0.74. Typo and
    quibble separate at 0.78 and 0.80.
  - Contrast is at least 6.06:1 on the soft fill and 4.52:1 on the blocking
    fill. Placement and names are unchanged.
  - The 1× renders were inspected pixel by pixel in all four variants,
    normal and blocking. The A reads as apex, shoulders, counter, bar and
    feet. The c's terminals are about three-quarter-strength pixels.

Limitations: static headless-Chrome frames, not native evidence (R33). Typo is
not approved until the owner accepts it.

Owner review: session `d7393d7131b629b535e95f60cfb9564b` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v004`,
ready at sequence 3). It asks one Accept/Reject question, `typo-v004`. It shows
v3, v4 and the owner's reference, plus the 1× marks enlarged. The frozen root
is `/Users/jaeger/bpr1080-label-icons-review-v004.1AmFcn`. The v003 session's candidates were released at its sequence 10, with
the verdicts kept, and its frozen root was deleted.
