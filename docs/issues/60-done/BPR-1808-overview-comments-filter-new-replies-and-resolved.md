---
schema: "implement-rfc.card.v1"
id: "BPR-1808"
title: "Overview's Comments filters to new replies and hides resolved threads"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1808--BPR-1808--192948dca553.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1781","BPR-1779","BPR-1290","BPR-1337"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "command", "command": "ctest -R '^(core_conversation_timeline|qt_github_conversation_panel|qt_workbench_overview|qt_workbench_conversation_reveal)$'", "detail": "qt_github_conversation_panel test_overview_comments_filter_new_and_resolved: in the pull request view the panel shows Only new and Hide resolved as chips in its band with no scope; Only new lists the two comments with new replies in time order; with Hide resolved too the resolved new thread goes; the cards view now passes Only new and Hide resolved to core arrange_conversation_timeline in that view (no drill-down list). Window (test_overview_drawer_borrows_the_conversation_tabs, run by qt_workbench_overview and qt_workbench_conversation_reveal): Overview's Comments shows both chips, off; Only new with nothing new empties it; Hide resolved keeps comments, which are never resolved."}}, {"criterion": "AC2", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "command", "command": "ctest -R '^(core_conversation_timeline|qt_github_conversation_panel)$'", "detail": "core filter_summary: whole/new/resolved/shown counts with no filter, Only new, Hide resolved; kept open thread counted as shown; an approval alone is no comment; equal inputs give equal summaries. Panel: '4 comments, 2 resolved, 2 with new replies' unfiltered and '2 of 4 comments shown, 2 with new replies' filtered; Only new carries the new count; Clear filters is offered only while a filter is on."}}, {"criterion": "AC3", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "command", "command": "ctest -R '^(core_conversation_timeline|qt_github_conversation_panel|qt_workbench_overview)$'", "detail": "core: no_new (nothing new, with or without Hide resolved), no_new_unresolved (the only new reply is on a resolved thread), no_unresolved, no_match (drill-down), none for an empty conversation. Panel: the list gives way to 'No new replies' / 'No new replies on unresolved comments' with Clear filters; Space on Clear filters turns both off, shows four comments and puts focus on the list; the band's Clear filters clears both without touching Review's scope. Window: Clear filters in the empty state shows the conversation again."}}, {"criterion": "AC4", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "command", "command": "ctest -R '^(qt_github_conversation_panel|qt_workbench_overview|qt_workbench_window_scale_100)$'", "detail": "Panel: Review's Hide resolved, on beside the code, is not Overview's; leaving gives Review its Hide resolved back (two icons on the tab row, File scope unchanged) and returning gives Overview its Only new; Discuss takes the panel with its own filters and leaves Overview's; the first-filter preference arriving while Overview holds the panel goes to Review's tray only; a new pull request starts unfiltered; a link to a comment the filters hide turns them off and the notice names them. Window: Overview's Hide resolved survives a trip to Review and back through the Pull request conversation link, with the open comment open again."}}, {"criterion": "AC5", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "command", "command": "ctest -R '^qt_github_conversation_panel$'", "detail": "Chips and Clear filters are StrongFocus and toggle by Space key events; chip accessible names 'Only new' and 'Hide resolved', descriptions naming comments; the empty message is the empty widget's accessible name and a polite QAccessibleAnnouncementEvent is sent when it appears, as is the reveal notice. No surface has filter shortcuts, so none were added."}}, {"criterion": "AC6", "outcome": "pass", "revision": "2101aedfdced3961467e79e3bbd2945403ea2f98", "evidence": {"kind": "artifact", "detail": "BPR_OVERVIEW_FILTER_RENDER_ROOT=<dir> better_pull_review_qt_github_conversation_panel_tests wrote filtered, no-new-replies and unfiltered renders in light and dark (neutral fixture: reviewer-one, reviewer-two, author-one) to the session scratchpad's overview-filter-renders directory, outside the repository. Forced-style offscreen renders are layout evidence only; the owner's look is not part of this card."}}]
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

## Built

- Core `summarize_conversation_timeline` in `conversation_timeline` counts
  the conversation as `arrange_conversation_timeline` lists it, before and
  after the filters, and names why a filtered list is empty
  (`timeline_empty`).
- The cards view gives the core Only new and Hide resolved in the pull
  request view too (a drill-down list stays Discuss's) and keeps the core's
  summary of each arrangement.
- `github_conversation_panel::set_pull_request_conversation` is how the
  window lends the panel to Overview: it swaps in Overview's own filters,
  shows Discuss's band with only the filter chips, counts and Clear filters,
  and shows the empty message with Clear filters in place of the list.
  `reveal_in_conversation` widens the filters before a link opens a hidden
  comment. The window calls these instead of the cards view directly.

## Follow-ups (not in this card)

- The chips are glyph-only with tooltips, as in Discuss. If the owner
  finds them hard to discover on Overview, words beside the glyphs are a
  small change.
- Filter state for all three surfaces still lives in the panel's buttons
  (as it did for Review and Discuss); moving it into a core state type would
  be its own card.
- The reveal notice stays until the next notice, as Discuss's does.
