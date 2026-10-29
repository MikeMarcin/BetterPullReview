---
schema: "implement-rfc.card.v1"
id: "BPR-1808"
title: "Overview's Comments filters to new replies and hides resolved threads"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1808--BPR-1808--192948dca553.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1781","BPR-1779","BPR-1290","BPR-1337"]}
acceptance: []
---

# BPR-1808: Overview's Comments filters to new replies and hides resolved threads

Owner, 2026-10-28: "Seems like I can't show new replies only or hide
resolved threads on the overview comments tab".

Review's Comments tab (two icons on its tab row), Discuss (two chips in its
band) and the Conversations finder all offer Only new and Hide resolved.
Overview's Comments tab ([[BPR-1781]]) holds the same conversation panel in
the pull request view, but the filter icons stayed on Review's tray header,
Discuss's band stayed hidden, and the cards view deliberately passed no
filters to the core's timeline in that view ("The Pull request view lists
everything"). So the tab had no way to narrow the conversation.

## Acceptance

- AC1. Overview's Comments shows Only new and Hide resolved as Discuss's
  chips over the conversation, with no scope control. Only new lists the
  comments with a post newer than what I have read (the existing read
  ledger); Hide resolved hides resolved threads; comments and review
  summaries are never resolved and stay. The open comment is never filtered
  out. The filtering is the core's `arrange_conversation_timeline`.
- AC2. The band counts the whole conversation ("4 comments, 2 resolved, 2
  with new replies"); filtered, it says how many of the whole are shown ("2
  of 4 comments shown, ..."). Only new carries the count of new comments.
  The counts are the core's `summarize_conversation_timeline`.
- AC3. When the filters leave nothing, the list gives way to a message the
  core chose: "No new replies", "No new replies on unresolved comments" (the
  new replies are all on resolved threads) or "No unresolved comments", with
  a Clear filters button. Clear filters (it or the band's) turns both off
  and shows the whole conversation; the button puts the keyboard on the
  list. An empty conversation is not a filter's doing and keeps its own
  wording.
- AC4. Overview's filters are its own: Review's tray filters and Discuss's
  filters are not changed by them and do not change them; each place gets
  back what it had. A new pull request starts unfiltered. The comments
  drawer's first-filter preference still applies to Review's tray only.
- AC5. Keyboard and accessibility: the chips and Clear filters take focus
  and Space; the chips' accessible names are their words, their
  descriptions say comments; the empty message is the panel's accessible
  name and is announced politely when it appears.
- AC6. Offscreen renders of the filtered, empty and unfiltered tab in light
  and dark exist outside the repository.

## Scope

The filter state, controls, counts, empty state and the list projection.
Card rendering, avatars and the drawer's tab row are unchanged. There are no
new shortcuts: no surface has filter shortcuts today. Outdated threads are
not hidden by Hide resolved, as on every other surface.
