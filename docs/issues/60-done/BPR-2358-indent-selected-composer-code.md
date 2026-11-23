---
schema: "implement-rfc.card.v1"
id: "BPR-2358"
title: "Indent selected code in the comment composer"
status: "Done"
rfcs: []
depends_on: ["BPR-2354"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2358--BPR-2358--32de348c47ea.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c11b35bda67c7839e39880682e0b8c949598645d","evidence":{"kind":"command","detail":"macOS arm64, Qt 6.12.0 Release, QT_QPA_PLATFORM=offscreen, qt_conversation_cards_view executable relinked with the candidate Qt object. Tab applied four spaces to each selected line in a suggestion, retained a reversed selection for a second press, and left the line at the end boundary alone. The same test object failed its first Tab assertion with the previous adapter."}},{"criterion":"AC2","outcome":"pass","revision":"c11b35bda67c7839e39880682e0b8c949598645d","evidence":{"kind":"command","detail":"macOS arm64, Qt 6.12.0 Release, QT_QPA_PLATFORM=offscreen, qt_conversation_cards_view executable relinked with the candidate Qt object. Shift+Tab removed a short indent and four columns of mixed tabs and spaces, preserved text at column zero, and kept selection and focus without adding an edit at the margin."}},{"criterion":"AC3","outcome":"pass","revision":"c11b35bda67c7839e39880682e0b8c949598645d","evidence":{"kind":"command","detail":"macOS arm64, Qt 6.12.0 Release, QT_QPA_PLATFORM=offscreen, qt_conversation_cards_view executable relinked with the candidate Qt object. Keyboard Undo and Redo reversed and reapplied each multi-line Tab or Shift+Tab as one action and restored original tab bytes."}},{"criterion":"AC4","outcome":"pass","revision":"c11b35bda67c7839e39880682e0b8c949598645d","evidence":{"kind":"command","detail":"macOS arm64, Qt 6.12.0 Release, QT_QPA_PLATFORM=offscreen, qt_conversation_cards_view executable relinked with the candidate Qt object. Tab and Shift+Tab traversed focus with a caret alone, selected prose, or a selection that touched fence text."}}]
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
