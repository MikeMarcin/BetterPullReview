---
schema: "implement-rfc.card.v1"
id: "BPR-1285"
title: "Break up the Share notes sheet with icons, tooltips and state colours"
status: "Parked"
rfcs: ["RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat note after handing back attended session 2dc9fa20 (BPR-1280/1281/1282 accepted), 2026-10-04","reservation":"docs/issues/_reservations/BPR-1285--BPR-1285--ac922798bca7.json","related_cards":["BPR-1282","BPR-1161","BPR-1180","BPR-1169"],"acceptance":["BPR-1285:icons","BPR-1285:chips","BPR-1285:hierarchy","BPR-1285:nothing-lost","BPR-1285:kept","BPR-1285:boundary","BPR-1285:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1285:icons","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"qt_review_cycle_view_1_0/_2_0: Copy, Save, Save as Bundle, Exact Text, Send to Agent and Use in External Agent carry glyphs; delivery lines carry a glyph per state; a refused route shows a Not allowed chip (Copy/Save) or the refused glyph (foot, refused agent in the Send list). unnamed_glyphs finds no shown glyph-bearing button without a tooltip and a name; negative control: Save as Bundle's tooltip removed is caught. Send course (cocoa, real harness): refused agent has the glyph, allowed none."}},{"criterion":"BPR-1285:chips","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Round chip (Not shared yet/state_stale, Changed since round N/state_fixed, Shared/state_accepted) and delivery tones (copied/saved state_accepted; read, adapter, acknowledged state_verified; uncertain syntax_warning; failed state_defect) equal code_role_colors_of(appearance); rendered pixels: copied line has accepted-role and no defect-role pixels, failed line the reverse, Shared chip accepted-role. Negative controls: tone mapped to another role fails; glyph painted in the quiet colour fails. Send course through the real composition on cocoa checks chip and onward observations in the appearance's roles."}},{"criterion":"BPR-1285:hierarchy","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"visible_sheet_words on the view rows' fixtures: 138 -> 63 before sharing, 266 -> 164 after a share with every observation kind (baselines measured at ed4c4cbd5 and asserted); introduction, caption, route uses, action meanings, order hint and goal purpose moved to info glyphs, tooltips and accessible descriptions; bold section headings; Send box still 7 words with 29 behind Details."}},{"criterion":"BPR-1285:nothing-lost","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"reachable() finds each of 16 former messages (frozen introduction, three action meanings, three route uses, exact-text tooltip and 'Read the Exact Text', caption, order hint, goal label and purpose, status, two delivery lines verbatim) on a shown control's text, tooltip, accessible name/description, info glyph or list line; draft and amended introductions checked by consequence(); negative control: a sentence never said is not found. Delivery item text equals the accepted words (RC61-06)."}},{"criterion":"BPR-1285:kept","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Existing view-row checks pass unchanged in meaning: one default Copy round N with Save beside it, the rest in the foot (direction A); Show Change words, place, link colour and keys; delivery words verbatim; every Tab stop ringed (info glyphs included); Return copies, Escape cancels, Control-C on macOS; Command-Return rows (BPR-1280); send course's harness-named one-line box and Details."}},{"criterion":"BPR-1285:boundary","outcome":"pass","revision":"7f221a426","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"build/dev: 69/69 cycle, share, send, suggestion, agent-launch, round-trip (pin unchanged), composed_application_hosts, test_network_isolation_manifest and architecture_boundaries rows; agent-free build/dev-tour-cli 72/72 of the same selection; clang-format clean; clang-tidy on changed lines only bugprone-signed-bitwise on Qt alignment flags (existing pattern). Linux and Windows are station requests."}},{"criterion":"BPR-1285:owner-look","outcome":"pending","revision":"7f221a426","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept/Reject BPR-1285-share-marks-v001 in session 2a1bd289e31810c0f149451f5e9bcc8e (ready at sequence 3) on the frozen root /Users/jaeger/bpr1285-share-marks-review-v001.CqAqeZ (git archive of 7f221a426, executable 52b04c6c...)."}}]
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

## Result (2026-10-04): done unattended; the owner's look pending

Landed as `7f221a426`. Built directly from the brief; no mockups were made.
The round's state is a chip beside the title, and the introduction is
behind an info glyph. The action words sit on the buttons' line. Copy and
Save stand side by side with glyphs, and when to use each is the tooltip. The
foot routes carry glyphs. A refused route is marked Not allowed, with the
reason as the tooltip. The status and the replace question sit on washed
panels. Shared so far shows the round, a glyph in the state's role colour,
the accepted words and the size; the caption is the heading's info glyph,
verbatim. Words on view fell from 138 to 63 before sharing and from 266 to
164 after. The view rows and the send course check names, tooltips, role
colours in data and pixels, and reachability of every former message, each
with a negative control. Integration is 69 of 69 in `build/dev` and 72 of 72
agent-free, with the round-trip pin unchanged.

Owner question `BPR-1285-share-marks-v001` in session `2a1bd289…` (ready at
sequence 3). Frozen root `/Users/jaeger/bpr1285-share-marks-review-v001.CqAqeZ`.
The full record and the Linux and Windows station requests are in the
ledger's BPR-1285 section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).
