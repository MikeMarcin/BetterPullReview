---
schema: "implement-rfc.card.v1"
id: "BPR-1271"
title: "Redesign the menu-bar Dif template image"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"Owner note on attended item BPR-1250-help-v003, session f18e0aba4e53e518cd4f57a4dbcfc475, event 8, decision a2b6a4dd-6865-4c03-bc4b-c87b3f5c2012, 2026-10-04T04:12:30Z","reservation":"docs/issues/_reservations/BPR-1271--BPR-1271--c1d270d5fab0.json","related_rfcs":["RFC-BPR-0031"],"related_cards":["BPR-1250","BPR-0766","BPR-0767"]}
---

# BPR-1271: Redesign the menu-bar Dif template image

## Origin

The owner accepted [[BPR-1250]]'s windowless menu bar (the app stays running
with Dif in the menu bar and Help in the windowless menus) and added, verbatim:

> It works, I'm not sure why but dif doesn't look great to me once it's actually there in the menu bar.

Source: session `f18e0aba4e53e518cd4f57a4dbcfc475` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261003-recent-v001`),
candidate `BPR-1250-help-v003` (fingerprint `51ea8a00…725895`, frozen build of
`4c8bcda0b` in `/Users/jaeger/bpr1250-review-v003.pDiQ9g`, executable sha256
`bf212edf…4b82a5`), event 8, decision
`a2b6a4dd-6865-4c03-bc4b-c87b3f5c2012`. The decision carries a screenshot of
the owner's dark menu bar with Dif at the left of the status items. It stays
in the session's event log and is not committed, because it shows other
applications' items.

The Accept stands on BPR-1250. Its criterion "the status item is a monochrome
Dif template image macOS tints" passed at `BPR-1250-menubar-dif-v001` and is
not reopened. This card carries only the look.

## Current state

- `scripts/dif_status_item_template.py` is the only maker of the image. It
  reads the accepted neutral master
  (`src/adapters/qt/assets/dif/source/dif-neutral-master.png`, sha256 pinned
  in the script), keeps the head down to the neck, inks the dark fur (ears,
  eye patches, eyes, nose and mouth) plus a one-point outline, and
  box-averages it to 18 x 18 and 36 x 36.
- The frozen render is the committed pair
  `src/adapters/qt/assets/status-item/dif-template.png` and
  `dif-template@2x.png`. `dif_status_item_template_contract` holds them
  byte-exact to the script, so a new look is a new recipe plus a deliberate
  `generate`, not a hand edit.
- `qt::menu_bar_status_icon` loads the pair with `setIsMask(true)`, and
  `qt_menu_bar_status_item(_cocoa)` checks mask, size and black-with-alpha.
- As drawn, the pale face is transparent, so on the bar it is the bar's own
  colour inside the outline. The eye patches and ears are the largest inked
  areas at 18 points.

## Outcome

A menu-bar glyph the owner judges reads well as Dif in the real menu bar, in
light, dark and highlighted bars, still a template image that macOS tints.

## Suggested approach

Run a design iteration with `frontend-design` or the
`sprite-sheet-animation-updater` skill. Compare candidate silhouettes at 16,
18, 20 and 22 points, at 1x and 2x, on light, dark and highlighted bars,
beside typical system status items. Candidates could include the head without
the outline, filled rather than hollow face shapes, fewer interior details, or
a simplified mark derived from Dif. Keep the result a reproducible recipe in
`scripts/dif_status_item_template.py`, the master's provenance and the
contract test. Ask the owner one Accept/Reject question on a frozen build
showing the chosen glyph in the menu bar, with the alternatives as media.

## Direct acceptance

- The owner accepts the new glyph as seen in the menu bar of a frozen build.
- `dif_status_item_template_contract`, `qt_menu_bar_status_item` and
  `qt_menu_bar_status_item_cocoa` pass on the new pair.
- `application_profile_empty_desktop_native_menu_bar` still passes in an
  Aqua session.
