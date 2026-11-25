---
schema: "implement-rfc.card.v1"
id: "BPR-2394"
title: "A proposed tour part's choices are a pencil, a trash and one worded action on one row, and the composed drawer course judges every state between two gestures: the proposal before Apply all was cut to \"dit tex\" because the course pressed past it"
status: "Done"
rfcs: ["RFC-BPR-0085", "RFC-BPR-0101"]
depends_on: ["BPR-2392", "BPR-2389"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2394--BPR-2394--36c16a3547fa.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"ec0c4e752","origin":"owner, attended review d3ddb6cf3847c406cf0604509af27568, Q4, 2026-11-25, rejecting the tour look with a screenshot of the window on screen; verbatim below","related_cards":["BPR-2392","BPR-2389","BPR-2384","BPR-2138","BPR-1925","BPR-1930","BPR-1915","BPR-2177"],"related_rfcs":["RFC-BPR-0085","RFC-BPR-0101","RFC-BPR-0092","RFC-BPR-0100","RFC-BPR-0106","RFC-BPR-0126"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7","AC8"],"owner_step":"attended look at the Tour tab's proposal before Apply all in a frozen tests-on build with the Fixture agent (see Owner judgement)"}
acceptance: []
---

# BPR-2394: A proposed tour part's choices fit one row, and the composed course judges every state between two gestures

## Why

Owner, attended review `d3ddb6cf3847c406cf0604509af27568`, Q4, 2026-11-25,
rejecting the tour look again, with a screenshot of the application's window
on screen (`owner-shots/04-tour-chat-real-window-0.png`): the Tour tab after
Draft Tour, two proposed stops, each with four buttons cut to "dit tex",
"Discard", "sk mor" and "d to to".

> It still looks exactly the same this must be an offscreen vs on screen
> render issue at this point if we're this many iterations deep. Further I
> think we could probable shorten those buttons, edit and discard have pretty
> standard pencil and trash can glyphs in the app. I do think Ask more should
> become ask follow-up though since that's what we use in the report screen.
> Actually I take that back, ask more here just opens the whole tour chat, so
> that shouldn't even be on the draft stops.

### It was not a render issue

The composed drawer course (BPR-2392) draws the right drawer from the
application's own window, off screen, under the native look. Held against the
owner's screenshot, its render of the same state (the proposal before Apply
all, a 280 px drawer, the display's own 200%) is the window on screen: of the
560 by 1124 pixels the two share, 99.64% are equal exactly, and the 2,248
that differ are the drawer's last two device columns, its one-pixel right
edge, where the window draws its neighbour
(`owner-vs-before-dark-200-tour-proposal-280-side-by-side.png` and `-diff.png`).

The course never drew that state for a look. Its `draft_tour` pressed
**Apply all** as soon as the draft was proposed and judged the route that
follows, so the proposal the owner sat in was neither measured nor drawn, and
every look showed a later state. A state is what the window shows between two
gestures, not only where a flow ends.

### What the course found once it stopped at each state

On main's `src/adapters/qt` (7f6dff82d), with the course and the Fixture
agent as here, each of four native runs (Dark and Light, at 200% and 100%)
reported 30 faulted state-and-width lines in seven of the twenty-eight
states, and none in the fifteen the course had before.

1. **A proposed part's choices** (`tour_agent_panel.cpp`): Edit text, Discard,
   Ask more and the part's action in a `QHBoxLayout`. Four buttons at their own
   widths need about 360 px; a proposed stop has 206 px of a 280 px drawer and
   346 px of a 420 px one. The layout gave each a quarter and the style cut
   the words. The same row on a stop's card (a proposed explanation: Edit
   text, Discard, Accept) was cut at 280 and 300 px, and with Show formatted
   at 340 px.
2. **Remove this stop?** on a stop's card: the question in a box layout with
   Keep and Remove was cut to "Remove this st" at 280 and 300 px.
3. **A stop's explanation field** showed a horizontal scroll bar and panned
   3 px at 340 px offscreen: Qt measures a block it lays out again as its
   widest line and some room more, so a line that nearly fills a wrapped
   field counts as wider than it.
4. **A proposed explanation's source on a stop's card** was cut at its foot:
   the pencil swapped the words for a taller field and the card kept the
   height the words had (seen in the renders; the course's checks did not
   yet look at a text field).

## Change

1. **A proposed part's row** is a pencil, a trash and its one worded action
   (Add to tour, Accept, Remove, Merge, Split, Reorder). The pencil and the
   trash are the comments' own glyphs as `quiet_tool_button`s, each named
   first in its tooltip and as its accessible name (Edit text, and Show
   formatted while the pencil is down and the Markdown source shows;
   Discard), reached with Tab and taking no keyboard on a press. Ask more is
   gone from a part Draft Tour proposed: the tour's own chat toggle opens the
   whole tour's chat. The row is a `detail::wrapping_row` whose last item
   trails (`set_last_trails`): the action keeps the row's right end, where a
   stop's own Accept stands, on one row and when a larger font wraps it.
2. **Remove this stop?** is a wrapping row: the question, then Keep and
   Remove at the right; in a card too narrow for all three the question keeps
   a line of its own. The two chips are laid out by the rect they paint.
3. **Wrapped prose fields** in the tour (a stop's explanation, a proposed
   text's source) have no sideways range (`detail::hold_wrapped_text_still`).
   A stop's card takes the height a part waiting on it asks for when that
   changes, so a source the pencil opens is whole.
4. **The composed course** walks the tour as a person does and judges the
   drawer after each gesture a person can stop at. Twenty-eight states (it was
   fifteen): the Tour tab with nothing drafted, Draft Tour running, its
   proposal waiting, a proposed text being edited, Reset tour asked and done,
   one proposed stop added while the other waits, a stop being written and
   its removal asked, a chat suggestion being edited, and a tour chat
   revision's parts waiting on the stops they change (a new explanation, read
   and edited, and a removal). It also refuses a button's face or a text
   field that a part holding it cuts (a row laid out shorter than its chips,
   a card still as tall as the words a field replaced), with a negative
   control for each. Before the question that follows a transcript replay it
   waits for the replay's card to go.
5. **The course's window is the window a person looks at.** It is told it is
   the active one before it is judged or drawn (the selected tab reads
   selected, a default button wears its accent), and the course takes the
   keyboard off what its own program-driven gestures left it on, so no button
   wears a focus ring a pointer-driven person would not see. Drawn natively
   without `QT_SCALE_FACTOR` it is at the display's own scale.
6. **Fixture agent**: the application hands an agent only the environment
   names its profile allows, so the course's `BPR_FIXTURE_AGENT_SLOW_STEP_MS`
   never reached it. A turn that must outlast a look is held open with a
   file, `hold-turns`, beside the course's own copy of the agent, and ends
   when the course removes it. "Rewrite stop N" in the whole tour's chat
   proposes a new explanation for that stop, which waits on the stop's own
   card, so a course reaches that state.
7. **Guidance** (`src/adapters/qt/AGENTS.md`): a state is between two
   gestures; a composed render is the window on screen, and what does differ
   (scale, the active window, the keyboard); how a course holds a turn; the
   trailing action row; wrapped prose fields.

Not in this card: the tour chat's own suggestion card keeps its worded
buttons (Accept, Edit text, Discard, Ask more), which wrap and were not in
the owner's note. The host's `ask_more_from` keeps its branch for a draft's
run, which the window no longer reaches.

## Acceptance

- AC1. A part Draft Tour proposed shows a pencil, a trash and Add to tour on
  one row at a 280 px drawer; no button is cut at 280, 300, 340 or 420 px or
  in the narrowest window; Add to tour is its one worded button and keeps the
  row's right end.
- AC2. The pencil and the trash are the profile's quiet icon tools with the
  app's existing glyphs, named in their tooltips and to a screen reader; the
  pencil shows the Markdown source, stays down while it shows, and says Show
  formatted.
- AC3. No Ask more on a part Draft Tour proposed.
- AC4. The composed course reaches and judges the proposal before Apply all
  and the other states between gestures, and fails if one is not reached.
- AC5. In every state the course reaches, at every width, at 100% and 200%
  and under the native look: no sideways pan, no control cut off or outside
  the drawer, no button narrower than its words, none over another, none cut
  by the part that holds it.
- AC6. The course's render of a state is the window on screen: held against
  the owner's screenshot of the same state, it differs only where the change
  is.
- AC7. Renders of these states, before and after, come from the composed
  window.
- AC8. Format, tidy on touched units; the documentation gate.

## Evidence

Revision `ec0c4e752` (on main 0835461b7, BPR-2393), macOS arm64 Debug, a
Retina display (the native look draws at 200% without `QT_SCALE_FACTOR` and
at 100% with 0.5). `ctest --test-dir build/dev -R '^<row>$'`; 24 of 24 rows
passed in one pass.

- AC1, AC2, AC3: `qt_tour_agent_session_view` passed with its new checks at
  280, 340 and 480 px: a proposed stop shows its pencil, its trash and Add to
  tour, left to right on one row, none narrower than it needs, the action
  within 16 px of the part's right end; the two tools are `quiet_tool_button`s
  with an icon and no words; the pencil is named Edit text in its tooltip
  and to a screen reader, is checkable, stays down and says Show formatted
  while the source shows, and survives a presentation of the same proposal;
  the trash is named Discard and discards its own part; Add to tour is the
  part's one worded button; no proposed part has an Ask more, with a host
  that would take one. `qt_tour_chat_in_tab` passed (Discard before the
  action on a proposed stop; the worded Tour actions are still one style).
- AC4, AC5: `application_composed_drawer_fit_scale_100`, `_scale_200` and
  `_cocoa` passed: twenty-eight states, "every state of the course is
  reached, in order", the negative controls first. Run by hand under the
  native look (Cocoa, QMacStyle, nothing on screen), Dark and Light at 200%
  and 100%, the course passed four times with no fault
  (`after-native-<scheme>-scale-<1|0.5>.log`).
- The same course and Fixture agent on main's `src/adapters/qt` (7f6dff82d),
  in report mode: 30 faulted lines in each of the four native runs
  (`before-native-*.log`), in tour-proposal, tour-proposal-editing,
  tour-reset-asked and tour-proposal-one-added (the four buttons, at every
  width under the macOS style), tour-revision-text-on-stop and
  -text-editing-on-stop (the same row on a stop's card, at 280 and 300 px,
  and at 340 px with Show formatted) and tour-stop-remove-asked (the
  question, at 280 and 300 px). Offscreen (Fusion) those sources reported 28,
  with the explanation field's scroll bar at 340 px. The before binaries
  predate the last check (a text field cut by its part); its negative
  control is what shows it catches one.
- AC6: `owner-vs-before-dark-200-tour-proposal-280-*`: the owner's screenshot
  and the course's render of main, 99.64% of pixels equal exactly, the rest
  the drawer's one-pixel edge. `owner-vs-after-dark-200-tour-proposal-280-*`:
  the same crop after, equal above the first row of choices and different
  from there down, where the row is now a pencil, a trash and Add to tour
  (86.21% equal: the taller row moves what is under it).
- AC7: 784 `before-*` and 784 `after-*` renders, all drawn by the composed
  course from the application's window, in
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/tour-proposal-renders/cocoa-macos/`
  as `<before|after>-<dark|light>-<100|200>-<state>-<width>.png`, with
  `...-window.png` for the whole window. Beside that folder: the eight runs'
  logs, the two `owner-vs-*` sheets with their difference images, and twenty
  `pair-<dark|light>-200-<state>-<width>.png` sheets (before beside after)
  of the states this change touches. The before renders are of main
  7f6dff82d; the after renders are of this revision.
- The other rows of the pass: `qt_tour_side_chat`,
  `qt_tour_side_chat_width_scale_100`, `_125` and `_200`, `qt_tour_stop_card`,
  `qt_tour_stop_card_cleanup`, `qt_tour_stop_acceptance_card`,
  `qt_review_tour_agent_failure`, `qt_drawer_surfaces_fit_scale_100`, `_125`
  and `_200`, `qt_button_faces`, `qt_button_faces_cocoa`,
  `application_tour_chat_queue_stop_public_path`, `_whole_tour_public_path`
  and `_local_draft_public_path`, `architecture_boundaries` and
  `test_network_isolation_structure_contract`. `fixture_agent_answers` passed
  with its new checks: "Rewrite stop 1" proposes that explanation, alone and
  with "Drop stop 2"; a turn waits while `hold-turns` stands beside the
  agent's own copy and answers once it is gone; the built agent, with no such
  file beside it, is not held.
- AC8: `format_check.py` (clang-format 22.1.8) passed on the eleven touched
  C++ files. `tidy.py --warnings-as-errors` over the seven touched
  translation units reports nothing on a changed line; its four findings are
  in lines this card does not change (`review_tour_editor.cpp:2196`,
  `fixture_agent_main.cpp:114`, `:149` and `:526`, the same four BPR-2392
  recorded). `scripts/verify_documentation.py --root .` printed
  `DOCUMENTATION_OK` and `tests/scripts/verify_documentation_tests.py` ran 82
  tests OK (both with the main checkout's `implement-rfc/scripts` on
  `PYTHONPATH`, which a worktree without the MASK-deployed skills needs).

Not covered: Windows and Linux have not run the changed rows. The proposal
row has no row of its own at 125% or 200% outside the composed course.
`test_network_isolation_manifest` fails in this partial build tree on
`gh_cli_git_smart_transport`, a row this card does not touch and whose
executable the tree did not build; this card adds no CTest row.

Two of some twenty-seven runs of the extended course stopped at the unchanged
question after the transcript replay ("the question is sent: What changed in
this push?"), with the Agent tab still showing the replay's card. The course
now waits for that card to go; none of the seven runs since (four native,
three CTest) stopped there. Seven runs do not prove a one-in-thirteen fault
gone, and the cause in the window was not found.

## Owner judgement

- The look of the proposal row (pencil, trash, Add to tour) is the owner's to
  accept, from a frozen tests-on build with the Fixture agent opened on a
  pull request whose tour has just been drafted; the `after-*` renders are
  that window's own drawing.
- Ask more is gone from a part Draft Tour proposed, as said. With it went the
  one gesture that seeded the tour chat's first question with the draft's own
  answer; the chat opened from the tour's toggle starts without that seed.
  Say so if the toggle should carry it.
- The tour chat's suggestion card still has worded Accept, Edit text, Discard
  and Ask more (they wrap). Say so if it should take the same pencil and
  trash.
