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
