---
schema: "implement-rfc.card.v1"
id: "BPR-1285"
title: "Break up the Share notes sheet with icons, tooltips and state colours"
status: "In Progress"
rfcs: ["RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat note after handing back attended session 2dc9fa20 (BPR-1280/1281/1282 accepted), 2026-10-04","reservation":"docs/issues/_reservations/BPR-1285--BPR-1285--ac922798bca7.json","related_cards":["BPR-1282","BPR-1161","BPR-1180","BPR-1169"],"acceptance":["BPR-1285:icons","BPR-1285:chips","BPR-1285:hierarchy","BPR-1285:nothing-lost","BPR-1285:kept","BPR-1285:boundary","BPR-1285:owner-look"]}
---

# BPR-1285: Break up the Share notes sheet with icons, tooltips and state colours

## Origin

After accepting `BPR-1282-send-box-v001` (session `2dc9fa20…`, event 12),
the owner said in chat, quoted exactly:

> the whole send window is just too full of text still, we need to break it up a little, icons, tooltips, colors, something

"The whole send window" is read as the Share notes sheet that hosts Send to
Agent, Copy, Save and the delivery list ([[BPR-1161]]'s direction A).
[[BPR-1282]] only shortened the Send box inside it.

## Outcome

The sheet reads at a glance. Prose that explains a control becomes an icon
on that control with the words in its tooltip and accessible description;
states (not shared yet, shared, changed since a round, refused, uncertain,
failed, and each delivery observation) are marked by a glyph and, where it
helps, a chip in the theme's role colours, always with words or a glyph
beside the colour; the sheet has clear sections, and long explanations sit
behind an info glyph, a tooltip or Details.

Kept as the owner accepted them: direction A (one default Copy round N with
Save as File beside it, the rest in the foot), Show Change and Show All
Changes, the meaning of every delivery observation and of the caption under
them (RC61-06, `BPR-1161-delivery-words-v001`: the words may move into a
tooltip but stay available and exact), the one-line Send box that names the
configured harness, the focus rings, and Return, Command-Return and Escape.

## Acceptance

- **BPR-1285:icons.** The routes (Copy, Save, Save as Bundle, the exact
  text, Use in External Agent, Send to Agent), the delivery observations and
  the refusal of a route carry glyphs. Every glyph-bearing control has an
  accessible name and a tooltip; a negative control fails when one is
  missing.
- **BPR-1285:chips.** The round's state and each delivery state are painted
  in the theme's role colours (`state_*` and `syntax_warning`), read from the
  appearance the sheet is given, never a fixed colour; each chip also has
  words or a glyph. A rendered check finds the role colour in the chip's
  pixels and fails against another role.
- **BPR-1285:hierarchy.** Fewer words are visible on the sheet before and
  after a share, measured on the same fixtures; the explanations moved to an
  info glyph, a tooltip or Details.
- **BPR-1285:nothing-lost.** Every message the sheet showed before is still
  reachable by pointer, keyboard and screen reader: the introduction, what
  each action asks for, when to use each route, each refusal, the delivery
  words and caption verbatim, the notes' order hint, the goal's purpose. A
  check lists them and finds each.
- **BPR-1285:kept.** The owner-accepted pieces above pass their existing
  rows unchanged in meaning.
- **BPR-1285:boundary.** The view rows, the send and desktop courses, the
  round-trip contract (pin unchanged) and `architecture_boundaries` pass.
- **BPR-1285:owner-look.** The owner's Accept/Reject on a fresh frozen build.
