---
schema: "implement-rfc.card.v1"
id: "BPR-1919"
title: "Code blocks in rendered tour text and agent answers are syntax highlighted"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1915"]
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: BPR-1915 look, 'it doesnt look like it was rendering with syntax highlighting'","related_cards":["BPR-1914","BPR-1915"]}
acceptance: []
---

# BPR-1919: Code blocks in rendered tour text and agent answers are syntax highlighted

## Owner (2026-11-06)

BPR-1915 accepted, with: "for the formatting one it doesnt look like it was
rendering with syntax highlighting".

## Change

Fenced code blocks (with a language tag, or the file's language where the
block cites a path) in the rendered Markdown of tour proposals, previews and
the tour conversation are highlighted with the app's existing syntax
highlighter, the one the diff view uses, and the same theme in light and dark.
Inline code stays plain monospace. The published GitHub form is unaffected.

## Acceptance

- AC1. A fenced `cpp` block in a proposal and in a conversation answer
  renders highlighted, matching the diff view's colours for the same tokens
  (Qt test on the rendered document's formats).
- AC2. An unknown or missing language renders plain monospace.
- AC3. Owner look: Accept/Reject (may share a session with BPR-1914 v002).
- AC4. The documentation gate passes.
