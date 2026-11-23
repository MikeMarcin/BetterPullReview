---
schema: "implement-rfc.card.v1"
id: "BPR-2358"
title: "Indent selected code in the comment composer"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2354"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2358--BPR-2358--32de348c47ea.json"}
---

# BPR-2358: Indent selected code in the comment composer

> I want block select tab and shift tab to work in code blocks on the comment composer to shift lines forward or backwards 4 spaces (tab width). Should work like a real editor meaning if text is already agaisnt the left coumn it doesnt move that line any futher. Undo redo should work after this like everything else.

The existing composer keeps Markdown and fenced code in one plain-text editor.
Use its code-line classification and native undo history.

## Acceptance

- AC1. Tab adds four spaces at the start of each line selected within a
  fenced block, including suggestions. A selection ending at the start of the
  next line leaves that line alone. The selection stays active for another keypress.
- AC2. Shift+Tab removes up to four leading columns from each selected line
  and stops each line at its own left margin. Leading tabs respect the
  four-column tab width. An outdent at the margin keeps focus and adds no edit.
- AC3. Each keypress that edits the text undoes and redoes as one action in
  the draft's existing history.
- AC4. With no selection, a selection in prose or a selection touching fence
  text, the keys retain normal focus traversal.

## Plan

Exercise the composed editor through its keyboard event path. Explain any
selection boundary decisions on the card, and verify repeated indent, clipped
outdent, undo, redo and focus traversal offscreen.
