---
schema: "implement-rfc.card.v1"
id: "BPR-1604"
title: "The quiet grey and the attention amber reach their contrast floors"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-18","owner_note":"Contrast debt: colours already below the floor before BPR-1585. Raise text to 4.5:1 and icons/badges to 3:1 on the plain background and the band, in all four themes, with the smallest change that keeps each colour's character; theme-derived adjustments, not hard-coded colours.","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-1085","BPR-1089","BPR-1156","BPR-1529","BPR-1585"],"reservation":"docs/issues/_reservations/BPR-1604--BPR-1605--1993453ea270.json","requested_skill":"frontend-design","milestone_plan":{"id":"bpr-polish-contrast-blocking","path":"docs/rfcs/plans/bpr-polish-contrast-blocking/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"command","detail":"Four-theme Files-palette tests strictly pass every text/letter/digit at 4.5:1 and every mark at 3:1 on Base and band; quiet labels also pass Window. Quiet Base/band/Window: Light4.90/4.54/4.58, Dark5.24/4.84/4.53, HC Light5.09/4.71/5.09, HC Dark5.46/5.05/5.46. Amber Base/band:4.87/4.51,7.49/6.92,6.32/5.85,12.63/11.69."}},{"criterion":"AC2","outcome":"pass","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"command","detail":"Core appearance contrast, actual Overview, appearance-profile and BPR-1529 Files-tree pixel tests pass. Existing readable inks remain unchanged; folders share exactly PlaceholderText and remain distinct from file names. Actual old Qt source baseline fails the new strict check."}},{"criterion":"AC3","outcome":"pending","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"artifact","detail":"Owner Accept/Reject pending in session 3b007b57637c66122767174fe1a527be, cursor 8; exact frozen root /Users/jaeger/bpr-1604-1605-polish-v002.0vxi9_2l, executable sha256 4e2a73bdd5b8d7d9c782b72a4e515c3825f784c70d006509e0ce2f0bb275ee66. Before/after renders embedded in the short session; full local handoff at /var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-1604-1605-review-6xa9fhl4/review-handoff.json."}}]
---

# BPR-1604: The quiet grey and the attention amber reach their contrast floors

The owner asked on 2026-10-18 to pay the contrast debt [[BPR-1585]] named
on its card: inks that were already below the floor before the Files band.

| Ink | Where | Light | Dark | HC Light |
|---|---|---|---|---|
| Quiet grey (`PlaceholderText`) | Note text, unknown "?" mark, folder paths | 3.16:1 | 4.15:1 | 4.0:1 |
| Amber | R badge, its letter, unresolved bubble | 2.52:1 | | 2.52:1 |
| Amber | open-chip digits | 3.77:1 | | |

Causes, read from the code:

- The quiet grey is the derived palette's `PlaceholderText`, the plain
  midpoint of text and base (`appearance_profile::derive`). The midpoint is
  below 4.5:1 on the base in Light, Dark and HC Light, and lower still on the
  Window surface, where much of the application draws it as label text.
- The R badge and the unresolved bubble draw `overview_colours::Marker`,
  which nothing ever assigns: it stays at its default `#D29922`, the Dark
  theme's amber, in every theme. Light's own amber (`state_attention`,
  `#AD7A00`) reaches 3.78:1, enough for a mark but not for digits or for the
  badge's letter.

Governing design: the Overview page of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), with
[[BPR-1585]]'s band, [[BPR-1529]]'s folder ink and [[BPR-1089]]'s chips.

## Design

Both inks are derived from the theme, by the least move that reaches the
floor, so a colour that already reads is untouched and a custom theme gets
the same treatment.

- Quiet grey: `core::readable_muted_ink`. The midpoint of text and base
  where it reads; otherwise the least move from it toward the text that holds
  4.5:1 on Base by the band's step (`core::quiet_band_contrast_hundredths`,
  so it still holds on the band), on AlternateBase and on Window. It stays a
  grey between the text and the ground, so it stays quiet; a midpoint that
  already reads on every required ground is unchanged. Every `PlaceholderText` use moves with it,
  including both folder inks, which stay the same ink ([[BPR-1529]]).
- Attention amber in the Files table: `core::readable_signal_ink`. The
  theme's `state_attention` where it holds the text floor on Base by the
  band's step; otherwise darkened (on a light ground) first in its weaker
  channels, so amber turns toward a burnt orange rather than the olive brown
  a plain darkening toward black gives, and only then toward black. The
  Overview's `Marker` (R badge, bubble) and open state (digits, round marks)
  both take it. Other `state_attention` uses (epoch line, progress marks,
  the Approve dot, the merge caution) are unchanged.

## Acceptance

- AC1: In Light, Dark, HC Light and HC Dark, Note text, the unknown "?" and
  folder paths reach 4.5:1 on the table's ground and on BPR-1585's band, and
  `PlaceholderText` reaches 4.5:1 on Window; the open-chip digits and the
  R badge's letter reach 4.5:1, the R badge and the unresolved bubble 3:1, on
  both. `test_band_keeps_every_ink_readable` holds every pair to its floor
  with no allowance for inks short before.
- AC2: Nothing regresses: every other ink that test measures keeps its
  floor; the folder ink stays exactly `PlaceholderText` in Overview and in
  Review's file tree and stays apart from file names; a theme colour that
  already reads is returned unchanged; the core functions are checked
  against the four built-in themes and a negative control.
- AC3 (human): The owner accepts the frozen candidate's Files table, before
  and after, in Light, Dark, HC Light and HC Dark.
