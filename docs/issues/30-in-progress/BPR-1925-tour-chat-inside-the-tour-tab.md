---
schema: "implement-rfc.card.v1"
id: "BPR-1925"
title: "The tour's side chat moves inside the Tour tab, beside the tour it's about"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1914"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07: the side chat felt decoupled from the tour; chose direction A from mockups (session a2c7c1b3)","related_cards":["BPR-1900","BPR-1913","BPR-1914","BPR-1919","BPR-1924"]}
acceptance: []
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
