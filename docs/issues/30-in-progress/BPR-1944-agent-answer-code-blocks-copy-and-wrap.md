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

## Implementation and review (2026-11-08)

The owner transferred the unfinished Claude worktree to Codex for direct
implementation, review, landing on main and cleanup after acceptance.
`frontend-design:frontend-design` and its desktop guidance were loaded.

The shared code store places a bounded widget over each fenced block's place
in the text document. The code stays selectable in the continuous document;
long lines scroll in the block's own read-only editor. The shared widget owns
its language header, keyboard-reachable Wrap and Copy, copied confirmation,
theme colours and syntax formats. Wrap is kept per block for the same answer.

The Agent tab and tour replies opt into this presentation. The Conversation
window now uses the same block store and widget while retaining its original
transcript, turn anchors, continuous selection and Copy turn/conversation.
Tour proposals keep the stop reader's look: they preview what will be
published rather than an agent answer.

Direct self-review found and corrected the missing Conversation surface,
layout markers leaking into selection Copy, a Windows SDK macro collision,
leading-tab expansion and the first fence's block format being lost when a
reply fragment entered the transcript. Empty top-level fences keep their
controls and copy an empty string. Fence-looking text inside another block is
validated with the existing scanner before inserting an empty-block marker.

The initial integration run passed 27/29. Its failures were an existing Debug
live-timer deadline (later semantic checks were unreached) and focus after the
new transcript-window check. The Debug course now allows a bounded three-second
watchdog for the live semantic update, consistent with the permanent Debug
timing policy; Release retains its 1.3-second bound. The window test explicitly
restores its main-window activation after hiding the transcript. These are
rechecked below; neither failure is relabelled a passing initial run.

AC5 remains open for the owner's exact frozen candidate. No landing or
worktree cleanup is claimed before that verdict.

## Automated verification

macOS arm64, Qt 6.12.0, Debug, 2026-11-08:

- AC1/AC4: `qt_markdown_code_1_0`, `_1_25`, `_2_0` exercise the visible
  Agent tab and its composed classifier; `qt_tour_side_chat` exercises the
  tour answer; `qt_tour_chat_in_tab` exercises the window's tour chat and
  Conversation window with the composed classifier. Header controls and
  language labels are present and the token colours are retained.
- AC2/AC3: `qt_markdown_body_1_0`, `_1_25`, `_2_0` verify exact clipboard
  text, empty blocks, Unicode/tabs, keyboard Wrap, independent wrapping,
  geometry and identity-bound wrap retention. `qt_tour_agent_session_view`
  verifies Conversation Copy/Wrap and selection spanning turns without
  layout-object characters.
- AC7: 400-character lines are tested in the Agent tab, tour chat and
  Conversation; each block scrolls horizontally and its containing message
  and pane have no horizontal scroll range.
- The integration selector covered 29 checks: architecture boundaries and
  their negative contract, composed GitHub, Markdown body, GitHub Markdown,
  CommonMark, HTML, composed Markdown, images, code and the three tour
  surfaces. Initial result 27/29, with the two failures retained above.
  After the corrections, the five affected checks (three code scales,
  side chat and composed tour chat) pass. Strengthening the Agent check to
  require actual visibility first exposed its fixture's missing configured
  agent; the fixture now offers an inert profile and the final three scales
  pass with a rendered visible answer. No process is launched by that profile.
- The built application, offscreen on `conventional-comments-v1`, with the
  existing tour-conversation course and a generated synthetic agent fixture,
  reaches `ATTENDED_TOUR_CONVERSATION_SHOWN`. Its rendered answer includes
  highlighted C++ and an independently scrolling 400-character plain line.
  An initial probe used an unsupported epoch phase and refused configuration;
  the corrected `single` phase passed. No live agent or GitHub is involved.
- `verify_documentation.py --root .`: `DOCUMENTATION_OK`.
  `verify_documentation_tests.py`: 67 pass. The Claude worktree lacks the
  ignored skill deployment link, so these commands use the installed
  implement-rfc scripts on `PYTHONPATH`.
- Clang-format 22.1.8 checks the 20 touched C++ files; focused clang-tidy 22
  passes for `markdown_code_block.cpp`, `markdown_code.cpp`,
  `markdown_body_view.cpp` and `tour_agent_panel.cpp`, warnings as errors.
  `git diff --check` passes. No dedicated Debug performance course is run.
- Evidence and direct-strategy continuation are outside Git at
  `/Users/jaeger/.codex/task-runs/bpr1944-kw0n7hzx`.

These are offscreen layout and interaction observations. Native appearance
judgment belongs to AC5; Windows and Linux native UI were not exercised.
