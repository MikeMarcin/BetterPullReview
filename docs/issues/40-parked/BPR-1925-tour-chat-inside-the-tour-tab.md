---
schema: "implement-rfc.card.v1"
id: "BPR-1925"
title: "The tour's side chat moves inside the Tour tab, beside the tour it's about"
status: "Parked"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1914"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07: the side chat felt decoupled from the tour; chose direction A from mockups (session a2c7c1b3)","related_cards":["BPR-1900","BPR-1913","BPR-1914","BPR-1919","BPR-1924"]}
acceptance: [{"criterion": "AC1 A part's chat opens inside the Tour tab, beside the tour, and the part is marked. No chat pane exists outside the Tour tab", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "qt_tour_chat_in_tab: in a real workbench_window the stop's glyph opens its chat as workbenchTourChatSplit's second column, to the right of the tour, with one tourSideChat in the window; the drawer widens to 700 or more; the route marks that stop (discussed role) and no other; the glyph is pressed. qt_tour_side_chat: no tab bar in the chat. The frozen look prints ATTENDED_TOUR_CHAT_PLACE:workbenchTourChatSplit:drawer=740.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#verification-v001-focused"}}, {"criterion": "AC2 Switching parts switches the chat. Every part's chat stays available and copyable", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "qt_tour_chat_in_tab: going to the next stop brings its chat and back brings the first with its turns; the Solution's glyph brings its chat. qt_tour_side_chat: one real click on the Solution's glyph brings its chat back with both turns, and Copy chat copies it whole.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#verification-v001-focused"}}, {"criterion": "AC3 Leaving the Tour tab hides the chat. The Tour tab's badge counts the parts with chats. Returning restores it", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "qt_tour_chat_in_tab: on Comments the chat is hidden, the drawer is 420 or less and workbenchTrayTourCount shows 2 (core::chat_threads, core_tour_agent_session); back on Tour the same chat shows at the wide width; a drag while the chat shows is remembered and the narrow width kept.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#verification-v001-focused"}}, {"criterion": "AC4 \"Update this part from this chat\" still yields a proposal, applied only on accept", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "qt_tour_side_chat: Update asks for the Solution rewritten from the chat, the proposal shows in the chat with Accept, and Accept dispatches the decision; nothing is applied before. The frozen look prints ATTENDED_TOUR_UPDATE and then ATTENDED_TOUR_SOLUTION, the accepted rewrite.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#verification-v001-focused"}}, {"criterion": "AC5 One click on any part or chat control doesn't scroll the tour (keep BPR-1914's click-scroll test green)", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "qt_tour_click_scroll passes on ec9eb504c.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#verification-v001-focused"}}, {"criterion": "AC6 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "ec9eb504c", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/.codex/attended-builds/bpr1925-tour-chat-in-tab-v001.r_ciekzw of ec9eb504c; new short session 59c3bc2a7ab71976b6b630ad892c4159, candidate BPR-1925-tour-chat-in-tab-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#attended-review-v001"}}, {"criterion": "AC7 The RFC amendment is written, and the documentation gate passes", "outcome": "pass", "revision": "ec9eb504c", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1925 supersedes BPR-1914 v004's window-level pane and tabs; review-tours.md updated; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1925-tour-chat-inside-the-tour-tab.md#implementation-v001-2026-11-07"}}]
parked_on: "human-review"
---

# BPR-1925: The tour's side chat moves inside the Tour tab, beside the tour it's about

## Owner (2026-11-07)

"it's kind of weird that the sidechat is so decoupled from the tour when it's
really about the tour." From three mockups, the owner chose **A. Chat inside
the Tour tab** (attended session a2c7c1b3, no note). The mockups are at
`docs/design/BPR-1914-tour-chat-directions-v1/` (direction A).

## Change

Load `frontend-design:frontend-design` first.

- Remove the window-level side chat pane. In its place, the Tour tab splits
  into a tour column and a chat column while a part's chat is open. The drawer
  widens (about 420 to 740 px in the mockup), and the width is remembered.
- The part under discussion gets the indigo agent rule. Other parts show their
  chat count on their glyph. Clicking a part's chat glyph, or "Ask about
  this", opens its chat in the column. Switching parts switches the chat. This
  replaces BPR-1914 v004's tab strip.
- Keep everything else:
  - "Update this part from this chat" and its BPR-1900 proposal (accept, edit
    or discard);
  - one replayed chat per part;
  - Copy chat;
  - "Worked for Ns";
  - BPR-1919 highlighting;
  - BPR-1916 `/skill` completion in the composer;
  - the BPR-1924 picker, once it lands.
- When the drawer moves to another tab (e.g. Comments), the chat goes with
  the Tour tab. A badge on the Tour tab shows how many parts have chats.
  Coming back restores the open chat.
- Close the chat column to get the narrow drawer back.
- Record in an RFC-BPR-0092 amendment that this supersedes BPR-1914 v004's
  window-level pane.

## Acceptance

- AC1. A part's chat opens inside the Tour tab, beside the tour, and the part
  is marked. No chat pane exists outside the Tour tab.
- AC2. Switching parts switches the chat. Every part's chat stays available
  and copyable.
- AC3. Leaving the Tour tab hides the chat. The Tour tab's badge counts the
  parts with chats. Returning restores it.
- AC4. "Update this part from this chat" still yields a proposal, applied
  only on accept.
- AC5. One click on any part or chat control doesn't scroll the tour (keep
  BPR-1914's click-scroll test green).
- AC6. Owner look: Accept/Reject on a frozen build.
- AC7. The RFC amendment is written, and the documentation gate passes.

## Implementation v001 (2026-11-07)

`frontend-design:frontend-design` was loaded first, for the three mockups and
again for this build. The mockups are committed at
`docs/design/BPR-1914-tour-chat-directions-v1/`; the owner chose A in session
`a2c7c1b32719b236c852fc3639fba697`. The one bold element is the agent's rule
on the part under discussion; the rest stays the drawer's quiet chrome.

- **Where it lives.** `workbench_window` no longer docks the chat. The Tour
  tab's page is a splitter (`workbenchTourChatSplit`): the tour's scroll area
  and `tour_side_chat`. The chat paints a line of the agent's accent at its
  left edge. It takes a little more than half the first time (9 to 11). On the
  tour's page (Expand) the chat moves beside the tour, and moves back on Back
  to the code.
- **The drawer's width.** `tour_chat_wide()` is true while the Tour tab shows
  an open chat. Then `central_sizes()` gives the drawer `TourChatWidth`
  (740 at first, clamped to 600 to 960), and `observe_drawer_width` keeps a
  drag as `TourChatWidth`, not as the host's narrow width. That width is Qt
  state for the window's life. The narrow width stays the host's, so a tab
  change or Close gives it back (`apply_drawer_widths` on the tab change).
- **The part under discussion.** `review_tour_editor::mark_side_chat` runs
  when the chat changes (`tour_side_chat::set_change_observer`) and when the
  stop card is placed:
  - The Problem's and the Solution's panels have a `part_marker` that paints
    a wash of the accent and its rule under them.
  - The route delegate paints the same mark on the stop's row and, for a stop
    with a chat, a chat glyph and its turn count beside the title.
  - Each part's glyph is checkable: it is pressed while its chat is open and
    shows its turns.
- **No tabs.** The `document_tab_strip`, +, the parts menu and the tab
  gestures are gone. The title row names the part ("Solution", "Stop 1, Where
  the change starts", "The whole tour") with Show, Copy chat and close.
  `open(thread, passage, aFocus)` brings a part's chat. While a stop's chat is
  open, going to another stop opens that stop's chat without moving the
  keyboard (`place_stop_card`).
- **Leaving the Tour tab.** The chat is inside the Tour page, so it goes with
  the tab. The Tour tab carries a `drawer_count_pill` (`workbenchTrayTourCount`)
  in the agent's accent (`set_colours`). It counts `core::chat_threads(session)`,
  the parts with an ask turn, each once (core, tested in
  `core_tour_agent_session`).
- **Narrow drawers.** `tour_agent_panel`'s head is a grid. Conversation and the
  tour's chat glyph keep together at the right of Draft the tour, or take their
  own row under it when both do not fit at their own widths (`fit_head`). The
  panel's minimum is the wrapped one. This fixes the pre-existing
  `qt_drawer_surfaces_fit_scale_100` failure (both buttons were 108 wide at
  280).
- **Look.** The `--attended-tour-conversation` look is unchanged and prints
  `ATTENDED_TOUR_CHAT_PLACE:workbenchTourChatSplit:drawer=740:chat=405:parts=2`
  before `ATTENDED_TOUR_CONVERSATION_SHOWN`.
- RFC-BPR-0092 Amendment BPR-1925 supersedes BPR-1914 v004's window-level
  pane and tabs, and `review-tours.md` says where the chat lives.

### Verification (v001, focused)

On `ec9eb504c`, dev build, 17 tests pass:
- `qt_tour_chat_in_tab` (new, a real `workbench_window`, dark and light). It
  checks that:
  - the stop's glyph opens its chat as the Tour tab's second column, with no
    chat elsewhere, and the drawer at 700 or more;
  - the route marks that stop and no other;
  - going to the next stop brings its chat, and coming back brings the first
    as it was;
  - on Comments the chat is hidden, the drawer is 420 or less, and the Tour
    tab shows 2;
  - back on Tour the chat is as it was;
  - a drag while the chat shows is remembered and the narrow width is kept;
  - the Solution's glyph brings its chat and marks it;
  - Close gives the narrow drawer back, still counting 2.
- `qt_tour_side_chat` (updated: no tabs; glyph pressed and counting; switching
  by one real click; Copy chat whole; Update's proposal accepted in the chat;
  Close keeps the chats).
- `qt_tour_click_scroll`.
- `qt_tour_agent_session_view`.
- `qt_drawer_surfaces_fit_scale_100`, `qt_drawer_surfaces_fit_scale_125` and
  `qt_drawer_surfaces_fit_scale_200` (the first was failing before).
- `qt_review_tour_steps`, `qt_review_tour_agent_help`,
  `qt_review_tour_agent_setup_call`, `qt_review_tour_agent_failure`,
  `qt_review_tour_stop_view`, `qt_tour_stop_card` and
  `qt_review_tour_publication_working`.
- `core_tour_agent_session`.
- `architecture_boundaries` and `architecture_boundary_contract`.
- `application_review_round_trip_composition_contract`, its app source pin
  resealed from its own output (`cc23f860…`).

`coding-format-check` passes with clang-format 22.1.8. The offscreen look on
the dev build reaches `ATTENDED_TOUR_CONVERSATION_SHOWN` with the line above.

### Attended review (v001)

- Frozen build `/Users/jaeger/.codex/attended-builds/bpr1925-tour-chat-in-tab-v001.r_ciekzw`
  of `ec9eb504c` (executable SHA-256
  `ef64eccf38a8cb2cca347c1a7ef383f12e128bb68d9e5304d937567125a3cddc`).
  Its launcher's own offscreen check reached
  `ATTENDED_TOUR_CONVERSATION_SHOWN`. The pictures in the session are that
  check's own (dark) and `qt_tour_chat_in_tab`'s.
- Launch: `python3 /Users/jaeger/.codex/attended-builds/bpr1925-tour-chat-in-tab-v001.r_ciekzw/launch.py`.
- New short session `59c3bc2a7ab71976b6b630ad892c4159`, state
  `/Users/jaeger/.codex/attended-review/bpr1925-v001-7WcXho/state`, host
  started with `serve --tailscale`.
- One candidate, `BPR-1925-tour-chat-in-tab-v001` (revision
  `ec9eb504c-v001`), with one Accept/Reject question.
- Status: ready, awaiting the owner.

### v001 rejected (2026-11-07)

v001 was merged to `main` as `9791aa4db`. The owner rejected the look in
session `59c3bc2a7ab71976b6b630ad892c4159`, verbatim: "The mockup had the chat
button on the right side of each of the sections and I prefer that, I also
want the chat button to toggle it open and closed."

They also reported that syntax highlighting was gone again in the Tour tab's
chat. Stop 1's answer showed its fenced block (`clear(); draw_scene();`) and
its inline code (`clear()`, `draw()`) in plain monospace.

The v001 host is stopped and its frozen root removed.

## v002 (2026-11-07)

`frontend-design:frontend-design` was loaded first. v002 is on branch
`bpr1925-v002-chat-toggle`, made from `origin/main` `9791aa4db`.

- **A toggle at the right of each part.** `part_chat_toggle` is a checkable
  `QToolButton` in the agent accent. It shows its chat's turns: an outlined
  speech bubble when off, a filled pill when on. One painter
  (`paint_chat_toggle`) draws it, so the route's painted toggles match the
  buttons. It sits:
  - at the right of the Problem's and the Solution's step rows, before
    Change (`tour_step_row::set_chat`);
  - at the right of the open stop's title row, after its pencil;
  - on every other stop's route row, where the route delegate paints it and
    `tour_outline` takes its click as the toggle's, not the row's
    (`route_chat_box`);
  - by Draft the tour, for the tour as a whole.

  The fields' helper rows no longer carry the glyph. The object names stay
  (`tourProblemChat`, `tourSolutionChat`, `tourStopChat`,
  `tourChatAboutTour`).
- **It toggles** (`review_tour_editor::toggle_side_chat`):
  - a click opens that part's chat (a stop's with its lines in the code);
  - on the part whose chat is open it closes the column;
  - on another part it switches.

  Space works it, it shows focus, and its accessible name and tooltip say
  whether it opens or closes the chat and how many turns it has.
- **Coloured code again.** The cause: when the highlighter has classified a
  text before, it answers inside the request, synchronously. A chat reader
  joined `Readers` only after `set_body` had asked, so every reader the chat
  made again (on each presentation of the session) missed that answer and
  stayed plain. Offscreen tests queue their answers, so they hid it. Now
  `tour_side_chat::reader` adds the view first, and `tour_agent_panel` does
  the same for its proposal readers.
- **The open stop's card fits.** The route never refitted the card when the
  chat narrowed the column, so its actions and the toggle ran past the edge.
  Now:
  - the card takes the route viewport's new width (an event filter, and
    `place_stop_card_now` comparing the whole size);
  - the tour and the chat share the drawer evenly at first;
  - while the chat is open the tour keeps 340 pixels and the drawer stays
    between 660 and 960.
- **A calm edit box for a stop's text.** The coordinator relayed the owner on
  the in-place editor: the blue box "feels cramping, I dont like it how it
  is". It had a thick blue focus border hugging the text, with no inner
  padding. Now:
  - The stop's `Body` has no frame and no native focus rectangle, and the
    editor's keyboard-ring pass leaves it out.
  - The stop documents have no document margin, so the words keep the
    reader's place and width.
  - `calm_text_box` paints on the card behind them: a box 8 pixels out at the
    sides and 6 above and below, the ground a shade off. Its hairline edge
    takes the focus colour, still one pixel, while the keyboard is in it.
  - Insetting the reader instead was tried first. It changed the reader's
    width, which set off a scroll-bar resize loop between the tour's
    `QScrollArea` and the editor, and that crashed
    `qt_drawer_surfaces_fit` with a stack overflow. That approach was dropped.
- RFC-BPR-0092's BPR-1925 amendment and `review-tours.md` say this.

### Verification (v002, focused)

On `0f6617456`, dev build, 18 tests pass.

`qt_tour_chat_in_tab` now covers, in a real window, dark and light:
- the reply's `cpp` block and its inline code coloured inside the Tour tab's
  column, when it first shows, after leaving the Tour tab and coming back,
  after a new presentation, when the answer comes inside the request
  (immediate lanes, which failed before the fix), and after the appearance is
  applied again;
- the toggles in the Problem's and the Solution's headers, at their right;
- the Solution's toggle opening, the Problem's switching, and the Problem's
  again closing the column, with the narrow drawer back;
- Space opening and closing the Solution's chat, with the accessible name
  following;
- the next stop's toggle on the route opening its chat, and the open stop's
  own toggle closing it;
- the pencil opening a stop's text with the keyboard in it, and that text:
  - has no native focus rectangle, no frame and no ring;
  - has its quiet box with room inside the card;
  - starts its first line where it was read (within two pixels across and
    three down).

The calm edit box, on the next commit, passes the same set and
`qt_scoped_prose_editor`: 19 tests.

`qt_tour_side_chat` reads the Solution's header toggle: in its row, at the
right, its name, pressed with its count.

The rest pass too:
- `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200`;
- `qt_review_tour_steps`, `qt_review_tour_agent_help`,
  `qt_review_tour_agent_setup_call`, `qt_review_tour_agent_failure`,
  `qt_review_tour_stop_view`, `qt_tour_stop_card` and
  `qt_review_tour_publication_working`;
- `qt_tour_click_scroll` and `qt_tour_agent_session_view`;
- `core_tour_agent_session`;
- `architecture_boundaries` and `architecture_boundary_contract`;
- `application_review_round_trip_composition_contract`, whose app source is
  unchanged.

`coding-format-check` passes, and `verify_documentation.py` prints
`DOCUMENTATION_OK`. The offscreen look on the dev build prints
`ATTENDED_TOUR_CHAT_PLACE:workbenchTourChatSplit:drawer=740:chat=368:parts=2`
and `ATTENDED_TOUR_CONVERSATION_SHOWN`.
