---
schema: "implement-rfc.card.v1"
id: "BPR-1915"
title: "Tour drafts and agent answers use real formatting instead of walls of text"
status: "Parked"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900"]
parked_on: "human-review"
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1900","BPR-1914","BPR-1916"]}
acceptance: [{"criterion": "AC1 A drafted Solution and stop from the stub (and from the installed Claude offscreen) come back with paragraphs, lists or inline code where fitting, and render that way in the proposal and the preview", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "Stub drafts and the installed Claude Code 2.1.310 offscreen (Draft the tour on the fixture: a Problem, a Solution and 12 stops) came back as short paragraphs with inline code; the proposal renders them (qt_tour_agent_session_view: a proposed list is a list, code is not backticks). The Tour tab preview renders the same Markdown with the same reader.", "path": "docs/issues/40-parked/BPR-1915-tour-drafts-use-formatting.md#verification"}}, {"criterion": "AC2 What the Tour tab shows matches what publishing to GitHub shows (a fixture comparison)", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "qt_markdown_github_* (markdown_github_tests check_tour_formatting): tests/fixtures/markdown/github-parity-v1/tour-formatting.md rendered by the Tour tab's Markdown view matches GitHub's own rendering (tour-formatting.github.html, from the Markdown API) in paragraphs, bullet and numbered items, inline code, bold and the code block; render_tour_stop publishes the narrative's bytes unchanged.", "path": "docs/issues/40-parked/BPR-1915-tour-drafts-use-formatting.md#verification"}}, {"criterion": "AC3 Owner look: shared with BPR-1914's session if timing allows, otherwise its own Accept/Reject", "outcome": "pending", "revision": "da676456a", "evidence": {"kind": "observation", "detail": "Same frozen build /Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud; session ac06de1b29f46bbf0be650bd3943fbf6, candidate BPR-1915-formatted-drafts, its own Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1915-tour-drafts-use-formatting.md#attended-review-ac3"}}, {"criterion": "AC4 The documentation gate passes", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1915-tour-drafts-use-formatting.md#verification"}}]
---

# BPR-1915: Tour drafts and agent answers use real formatting instead of walls of text

## Owner (2026-11-06)

"having used it for a while I want to do more than just enhance the tour text with my agent. I want to ask questions, get back and forth and have it update it. For example right now I have Solution section where the draft said something I don't understand, but I don't really have a way to ask for clarification. It also feels like the output of the draft is entirely devoid of any formatting. It feels just like big walls of text. I have a skill here for /technical-book-humanizer but I also have a skill elsewhere for technical writing that is slightly more tuned to those purposes. I wonder if I can't have a hook to let me reference a custom skill in my prompts if the skill is available in as a personal user skill or a skill in the repo being reviewed. I also wonder if we can improve the settings agent tab where we're editing the built-in prompts its a bit weak maybe some /frontend-design:frontend-design could improve that."

## Change

Load `frontend-design:frontend-design` first.

- Find what the tour's published form and the Tour tab support (GitHub-flavoured Markdown in the published tour? what the editor and preview render?). Make drafting prompts ask for the structure that renders there: short paragraphs, bullet lists where they help, inline code for identifiers and paths, fenced code only when short, and no headings inside a stop unless supported.
- If the Tour tab's fields show raw Markdown as plain text, render it in the proposal, the preview and the conversation (BPR-1914), and keep the source editable.
- The built-in drafting prompts include a short style section (plain, precise, scannable). This is the default the BPR-1916 skill references can replace.
- Keep length bounds, so a stop stays short.

## Acceptance

- AC1. A drafted Solution and stop from the stub (and from the installed Claude offscreen) come back with paragraphs, lists or inline code where fitting, and render that way in the proposal and the preview.
- AC2. What the Tour tab shows matches what publishing to GitHub shows (a fixture comparison).
- AC3. Owner look: shared with BPR-1914's session if timing allows, otherwise its own Accept/Reject.
- AC4. The documentation gate passes.

## Implementation (2026-11-06)

`frontend-design:frontend-design` was loaded before the UI work.

- **What renders where.** Tour fields are GitHub-flavoured Markdown. The Tour
  tab's Preview and stop reader render them with `markdown_body_view` (the
  reader GitHub parity is tested against), and publishing posts the same
  bytes (`render_tour_stop` keeps the narrative as written). The editing
  fields stay source editors.
- **Style section.** `application::tour_writing_style()` is one named piece
  carried by Draft the tour, the ask turns (BPR-1914) and the writing hooks
  for the Problem, the Solution and a stop's comment (not the outline). It
  asks for short paragraphs, bullet lists for parallel points, inline code
  for names and paths, a fenced block only for at most six lines, no
  headings, tables, HTML or images, and the existing length bounds. It is the
  default a BPR-1916 skill reference can stand in for. Its own file keeps it
  apart from BPR-1916/1917's prompt work.
- **Rendered in the proposal and the conversation.** Each pending proposed
  text shows rendered (an in-place `markdown_body_view`, as tall as it reads).
  **Edit text** shows the Markdown source to change before Accept, and
  **Show formatted** renders the edit. Accept sends the source. Conversation
  replies render through the same import
  (`detail::import_github_markdown`).
- **Fences inside fields.** An answer is read as JSON first, then from a
  fenced block. A Problem or Solution holding a fenced code block no longer
  breaks the whole answer (`decode_tour_draft_answer`, `decode_tour_ask_answer`).
- **Stub drafts** come back formatted: paragraphs, a list and inline code.

## Verification

- AC2 fixture: `tests/fixtures/markdown/github-parity-v1/tour-formatting.md`
  and GitHub's rendering of it, `tour-formatting.github.html`
  (`gh api markdown -f mode=gfm`, 2026-11-06). `check_tour_formatting` in
  `markdown_github_tests.cpp` (the `qt_markdown_github_*` courses) compares
  paragraphs, bullet and numbered items, inline code, bold and the code
  block in order. It passes.
- Installed Claude Code 2.1.310 offscreen, Draft the tour on the fixture
  through the dev build: "Proposed a Problem, a Solution and 12 stops." All
  came back as short paragraphs with inline code (`Renderer::resize`,
  `uProjection`, `--release`), and the proposal rendered them.
- `application_tour_agent_conversation` (the style section rides the draft
  and ask requests; fenced fields read), `application_writing_delivery`,
  `application_agent_launch`, `qt_tour_agent_session_view` (rendered list,
  no backticks, Edit text, Accept sends source): pass.
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.

## Attended review (AC3)

Shared with BPR-1914's session. Frozen build `/Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud` of `da676456a` (executable SHA-256 `0f475b2065f4ce0b31ae142b09a839685668bb02acbc51e39ee812ae92535051`), fixture
`conventional-comments-v1` on `src/engine/renderer.cpp`, stub agent
`attended-drafting-claude`, ready line `ATTENDED_TOUR_CONVERSATION_SHOWN`; the
launcher's own offscreen check reached it. Launch: `python3 /Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud/launch.py`. Pictures from
that executable, offscreen, light and dark. New short session `ac06de1b29f46bbf0be650bd3943fbf6`,
state `/Users/jaeger/.codex/attended-review/bpr1914-v001-HmfYOg/state`, host started with `serve --tailscale`. Status: ready,
awaiting the owner. Candidate
`BPR-1915-formatted-drafts` (revision `da676456a`), its own Accept/Reject
question.

The frozen commit `da676456a` stays reachable on the local branch
`bpr1914-v001-frozen-history`. The card's branch was then rebased onto `main`
past BPR-1908, which moved the tour session code into
`src/app/src/github_application/window_tours.cpp`, so the ask code now lives
there. On the rebased commit the 16 focused tests, the format check, the
documentation gate and the offscreen look (`ATTENDED_TOUR_CONVERSATION_SHOWN`)
pass again, with the composition pin resealed.
