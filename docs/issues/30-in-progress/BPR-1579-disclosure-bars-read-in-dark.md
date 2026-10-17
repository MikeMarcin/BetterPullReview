---
schema: "implement-rfc.card.v1"
id: "BPR-1579"
title: "Disclosure bars read in every theme"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner bug 2026-10-17","owner_note":"In Dark, Overview's \"Check details: 3 failed\" disclosure bar draws white text on a light-grey fill (low contrast, a \"flashbang\"). \"Requirement details\" and \"Pull request and account\" (Compose review) look affected too.","related_cards":["BPR-1513","BPR-1157","BPR-0207"],"reservation":"docs/issues/_reservations/BPR-1579--BPR-1579--a8fa7b7c4cd6.json","accepted_treatment":"BPR-1513 v003 (session 0fec6737, decision d96dea45)"}
---

# BPR-1579: Disclosure bars read in every theme

The owner reported on 2026-10-17 that in Dark the Overview's "Check details:
3 failed" bar draws white text on a light grey fill, the same flash
[[BPR-1513]] removed from the Appearance banners, and that "Requirement
details" and Compose review's "Pull request and account" look affected too:
grey bordered buttons with a chevron.

Every one of these is a plain `QToolButton` with an arrow, so the platform
style draws it. On macOS that is a native bezel whose grey follows the
system appearance rather than the profile, under the Dark profile's
near-white text.

This is a bug fix that applies BPR-1513's accepted v003 treatment (the
theme's raised surface, `QPalette::Light`: Light `#FFFFFF` on `#F6F7F9`, Dark
`#2A3039` on `#1C2026`; the high-contrast themes flat). It adds no new design:
layout and behaviour stay, and the UX redesign may restyle these panels later.

## Acceptance

- AC1: One shared disclosure bar paints every chevron disclosure from the
  theme's surface and text roles, with no style sheet and no second palette.
  The bar is `QPalette::Light`, or the plain surface where a palette's
  `Light` does not read under its text. The title and chevron are the text
  colour.
- AC2: Each affected disclosure, in the four built-in themes, closed, open
  and pressed, renders its title at 4.5:1 or more on the bar, the bar as the
  theme's raised surface and the chevron in the text colour. A negative
  control fails the same reading on the old platform-drawn bar.
- AC3: Behaviour is unchanged: object names, accessible names and
  descriptions, checked state, the arrow type the composed tests read, and
  what each disclosure shows and hides. The focused bar draws the window's
  focus ring.
- AC4: Focused tests, one full `ctest --preset dev` on the merged tree,
  clang-format 22, tidy and the documentation gate pass. Before and after
  renders in Light and Dark are recorded on this card. No new owner review
  is needed because the treatment is the one the owner already accepted.

## Coordination

The frontend-design skill is loaded for a consistency check. The Overview
Open and All switch ([[BPR-1577]]) and the review bar's Updates disclosure
(already painted from the palette) are not part of this card. The owner's
desktop is never driven or captured.
