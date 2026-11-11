---
schema: "implement-rfc.card.v1"
id: "BPR-2072"
title: "Quiet buttons draw on their own surface on every review surface"
status: "Done"
rfcs: []
depends_on: ["BPR-2060"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2072--BPR-2072--7c516344897b.json","source_commits":["2c9264ada","b6936141d","bbc347442","ee2dac2fe","84a650296"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"84a650296","evidence":{"kind":"command","detail":"qt_button_faces and qt_button_faces_cocoa sweep auto-raise and flat buttons outside toolbars in four themes, default and macOS style under light and dark system: worst cocoa reading 1.00-1.09 before, at least 3.86 icons and 4.56 words after."}},{"criterion":"AC2","outcome":"pass","revision":"84a650296","evidence":{"kind":"command","detail":"Checkable Wrap and label toggles show a raised bordered on face at 3:1 and report checked; 84 of 84 affected Qt tests, architecture_boundaries, format and tidy pass."}}]
---

# Quiet buttons draw on their own surface on every review surface

Follow-up to BPR-2060 covering code block Wrap/Copy, the agent tab, the label
menu, overview, review cycle, tour, source search, submission editor,
workbench, tab strip, finder, notes, appearance and other auto-raise or flat
buttons outside real toolbars. A shared sweep test measures every such button
against its painted face.

## Acceptance

- AC1. Every swept quiet button meets the text or non-text minimum against its
  painted face under the native macOS style with either system appearance.
- AC2. Checkable quiet buttons show a contrasting on state and expose it to
  accessibility.

Known shortfalls: high-contrast system link blue on About (profile does not set
`QPalette::Link`); non-auto-raise native buttons and the overview status drawer
tabs keep native bezels; several converted buttons are hidden in sweep fixtures.
