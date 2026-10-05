---
schema: "implement-rfc.card.v1"
id: "BPR-1080"
title: "Develop all twelve reusable label icon candidates"
status: "Parked"
rfcs: ["RFC-BPR-0050"]
depends_on: ["BPR-1079"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-06","milestone":"design","acceptance":["R30","R31"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1080:roles","outcome":"pass","revision":"c93008559","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-frames-v1.html","detail":"Twelve distinct glyphs, one per catalog ID, no alias or omission (GLYPHS/OPTIONS); alternatives only for praise, chore, quibble. ?check=1 on the frozen source: min cross-label ink-overlap separation 0.538 at 12 px, 0.483 at 24 px; suggestion/polish 0.793; nitpick/quibble 0.694 (swap) and 0.864 (plus-minus)."}},{"criterion":"BPR-1080:previews","outcome":"pass","revision":"c93008559","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-v1.html","detail":"Actual 16 px marks normal and blocking in light, dark, high-contrast light and dark, 2.5x, greyscale and grid; thread cards at 1440x820 and 960x760 light/dark reading and editing; Overview chips, label menu, completion, Label key. Rendered at 1x and 2x (32 renders). Accessible name 'label (decoration)' on each mark image, tooltip, keyboard Label key; glyph contrast >= 6.06 soft and >= 4.52 blocking. Static frames, not native evidence (R33)."}},{"criterion":"BPR-1080:provenance","outcome":"pass","revision":"c93008559","evidence":{"kind":"artifact","path":"docs/design/BPR-1080-label-icons-v1.html","detail":"Coordinates hand-written by the rfc-implementer for this card; no icon set, font, tracing, image generation or chat sketch; repository-owned. Resource form: QPainterPath data (M/L/C/Z plus discs) compiled into the Qt adapter; no SVG, Qt SVG, icon font, runtime fetch or build-time generation. Nothing asserted approved."}},{"criterion":"BPR-1080:owner-selection","outcome":"pending","revision":"c93008559","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0047-0051.md","detail":"Owner verdicts pending in attended session 82eb35d69600801ab5f8875d88d836a2 (state /Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1080-label-icons-v001, ready at sequence 12): praise, chore, quibble (choices) and set (Accept/Reject), all -v001, on frozen root /Users/jaeger/bpr1080-label-icons-review-v001.BP1nCI."}}]
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
