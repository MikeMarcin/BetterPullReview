---
schema: "implement-rfc.card.v1"
id: "BPR-1944"
title: "Code blocks in agent answers get a header with the language, Wrap and Copy"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-08, coordinator chat, with a reference screenshot of another desktop chat app's code block header","related_cards":["BPR-1919","BPR-1941"]}
acceptance: []
---

# BPR-1944: Code blocks in agent answers get a header with the language, Wrap and Copy

## Owner (2026-11-08)

"Can we make it so that code blocks in agent responses in both agent and
tour side chat get a copy block and enable word wrap button in the upper
right."

The reference: a rounded code block with a header row showing a code glyph
and the language name ("Plain text") on the left. On the right are a
wrap-toggle icon and a copy icon (tooltip "Copy code"). Below is the code in
monospace, not wrapped by default and scrolling horizontally.

## Change

Load `frontend-design:frontend-design` first.

- Every fenced code block in rendered agent answers, in the Agent tab chat
  and in tour chats, gets a header row:
  - **Left:** a code glyph and the language name ("Plain text" when it has
    none).
  - **Right:** a **Wrap** toggle (tooltip "Wrap lines" / "Don't wrap") and a
    **Copy** button (tooltip "Copy code"; it briefly confirms "Copied").
- Unwrapped by default, with horizontal scrolling. Wrap toggles per block.
- Copy puts exactly the block's code on the clipboard: no fence, no
  language line, no trailing newline added.
- Keep BPR-1919's syntax highlighting. Use the shared Markdown rendering
  path both chats use, so it works everywhere answers render: the Agent tab,
  the tour chat column, and the Conversation window. Decide whether tour
  proposals' code blocks get it too, and record why.
- Theme tokens only, light and dark. Keyboard reachable, with accessible
  names.

## Owner addition (2026-11-08)

"It would also be good if those blocks were the width of the pane but
scrolled independently like codex instead of the whole pane expanding."

- Each code block is exactly the chat column's width. Unwrapped, a long line
  scrolls sideways inside that block only, by its own scroll bar or the
  trackpad. The chat pane and the message never widen and never get a
  pane-level horizontal scroll bar because of a long code line. This holds in
  the Agent tab chat, tour chats and the Conversation window.

## Acceptance

- AC1. A code block in an Agent tab answer and in a tour chat answer shows
  the header with its language, Wrap and Copy (Qt test).
- AC2. Copy puts exactly the code on the clipboard (Qt test reads it).
- AC3. Wrap toggles wrapping for that block only (geometry test).
- AC4. Highlighting is unchanged.
- AC7. A block with a 400-character line keeps the message as wide as the
  column; the block scrolls inside itself, and the pane has no horizontal
  scroll range (geometry test).
- AC5. Owner look: Accept/Reject on a frozen build.
- AC6. The documentation gate passes.
