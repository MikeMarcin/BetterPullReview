---
schema: "implement-rfc.card.v1"
id: "BPR-2355"
title: "The composer's code box is coloured as the reader colours code, and its header picks the block's language"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2354"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner, 2026-11-21, attended review session 4708604404524750554d23a4282d7890","placeholder_id":"allocate after the 0.5.0 freeze; replace BPR-2355 everywhere on the branch","related_cards":["BPR-2354","BPR-1562","BPR-1564"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2355: The composer's code box is coloured, and its header picks the block's language

## The owner's words

2026-11-21, accepting [[BPR-2354]]'s code box: "can we do syntax
highlighting? Also if we could make the language into a picker and hide the
``` that'd be great, but i dunno if we can do that while keeping it one text
block. If that's hard we can leave it at this."

## Change

- **Colouring.** The composer field (the draft's and a reply's) asks the
  reader's syntax service for each code block's colours, through the card's
  own code request ([[BPR-1562]]'s `markdown_code_syntax_request`). It asks
  300 ms after typing pauses. A request is keyed by the block's text and its
  selector, the file name the fence's language names
  (`markdown_code_language_of`). A ```` ```suggestion ```` block is
  classified by the commented file, as the reader's suggestions are
  ([[BPR-1564]]).
  - An answer comes back through the card's `apply_code_syntax`. The field
    keeps the last sixteen.
  - `fenced_code_highlighter` lays each code line's spans over its code font
    in the review panes' colours (`markdown_code_theme`), through
    `code_syntax_format_ranges_at`.
  - A line keeps its colours while it is unchanged at the same place, so
    typing in one line does not blank the rest before the new answer comes.
  - The words and the fences keep their own colours.
  - No dependency on `claude/context-aware-code-highlighting`. Its file
    anchoring can choose the selector later.
- **Language picker.** Each block's header row ends in a quiet picker, an
  `InstantPopup` `quiet_tool_button`. It names the block's language ("C++",
  "Suggested change", "Plain text"), and its accessible name is "Language:
  C++". Its menu lists Suggested change and sixteen languages. A choice
  rewrites the opening fence's info string as one undoable edit and leaves
  the rest of the text exactly as it was. A suggestion can be made a C++
  block, or the other way round, this way.

## Hiding the fences: evaluated, not done

The fences stay visible, muted, as the box's header and footer rows. In one
plain-text document a hidden fence is still text in the draft: an invisible
block, or glyphs drawn in no colour. Hiding it soundly would mean changing
editing itself, not just drawing:
- The caret would have to skip the fence rows by keyboard and by mouse.
- Backspace at the start of the first code line would merge the code into
  the hidden fence and silently end the box. Delete at the end of the last
  code line would do the same to the closing fence.
- Selections that start or end inside a fence would need care.
- Input methods composing beside a fence, and screen readers, would still
  meet the backticks the field no longer shows.

Each of these is a special case in the one field everything else writes
through (the label controls, Enhance, Update draft, the BPR-2298 range
promotion). The owner allowed leaving it ("If that's hard we can leave it at
this"), so it is left. The picker gives the choice of language without it.

## Acceptance

- AC1. After typing pauses, a code block's text is asked for once, with the
  selector of its language. A changed block is asked for again.
- AC2. The answer colours each code line in the theme's colours. The words
  and the fences are not coloured. An unchanged line keeps its colour while
  another line changes.
- AC3. The header's picker names the block's language. Choosing Python
  rewrites only the fence's language, one undo takes it back, and a block
  can be made a suggestion.
- AC4. `qt_conversation_cards_view`, `qt_composed_github_scale_100`,
  `qt_workbench_window_scale_100`, `qt_item_side_chat`,
  `qt_tour_side_chat`, `architecture_boundaries`, `coding-format-check` and
  the documentation gate pass.

## Verification

Run on macOS, dev preset, in a task build tree. See the branch's commit for
the exact revision.

| Criterion | Check | Result |
|---|---|---|
| AC1–AC3 | `qt_conversation_cards_view`, `test_a_code_box_is_coloured_and_names_its_language` | see commit |
| AC4 | as listed | see commit |

## Remaining

- The owner's look at a frozen fixture build with the real syntax service.
  The test feeds the field a classification. One question about the
  colours, one about the picker.
- The composed courses that run the real syntax service through the
  composer were not built here. They are for the landing round.
