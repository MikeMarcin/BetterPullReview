---
schema: "implement-rfc.card.v1"
id: "BPR-1915"
title: "Tour drafts and agent answers use real formatting instead of walls of text"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1900","BPR-1914","BPR-1916"]}
acceptance: []
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
