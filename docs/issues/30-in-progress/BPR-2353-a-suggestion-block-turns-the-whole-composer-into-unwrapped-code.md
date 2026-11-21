---
schema: "implement-rfc.card.v1"
id: "BPR-2353"
title: "A suggestion block turns the whole comment composer into unwrapped code"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","target":"0.5.0","origin":"owner, 2026-11-21","placeholder_id":"allocate after the 0.5.0 freeze; replace BPR-2353 everywhere on the branch","related_cards":["BPR-2271","BPR-2273","BPR-1590","BPR-2352"],"related_rfcs":["RFC-BPR-0090"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2353: A suggestion block turns the whole comment composer into unwrapped code

## The owner's words

2026-11-21: "there is a bug in the comment composer: as soon as you make a
```` ```suggestion ```` block in a comment draft that whole comment gets
rendered as an un-word-wrapped block including things outside of the code
block. If we want to give special rendering to code blocks inside the
comment composer I'm not against it but right now suggestion is the only one
that gets special treatment and its special treatment is wrong for things
outside that block."

## Cause

`open_thread_card::present_draft` (`conversation_open_card.cpp`) styled the
whole field from one flag. Whenever the draft had a suggested change
(`draft_composer_presentation::Change`, set when the body holds a
```` ```suggestion ```` block), it set the composer field (`threadDraftText`)
to the code font, `QPlainTextEdit::NoWrap` and a horizontal scroll bar. The
comment's words before and after the block went monospaced and unwrapped
with it, and the drawer could pan sideways in the field. Every other fenced
block, such as ```` ```cpp ````, got no styling at all.

The caret logic had a second fault. It put the caret on the body's last line
break, which assumed the body ended with the block. With words after the
block, the caret landed on the blank line before them.

## Change

- The field keeps its own font and wraps at its width in every case. It has
  no sideways scroll, as no drawer may pan sideways (`src/adapters/qt/AGENTS.md`).
- A new `fenced_code_highlighter` (`src/adapters/qt/src/`, a
  `QSyntaxHighlighter`) styles each fenced block's own lines and nothing
  else. That covers a ```` ```suggestion ````, ```` ```cpp ```` or any other
  language, and backtick or tilde fences of three or more, as GitHub reads
  them. The fences are in the drawer's code font in the palette's
  placeholder colour. The lines between them are in the code font. A block
  left open runs to the end of the text, as GitHub renders it. The
  highlighter changes only the layout, never the text, so typing still
  sends one draft edit per change. The field and the reply field get it from
  `reply_text::set_code_font`. A palette change restyles it, and the field's
  height counts the code lines' own heights.
- A suggestion's caret goes to the end of the block's last line, whatever
  follows the block.

### Decisions

- The frontend-design skill was loaded for this surface. The design is
  deliberately quiet: the code font and muted fences, and no painted ground
  band. `QPlainTextEdit`'s layout cannot inset single lines, so a band would
  put the code's first glyphs against its edge.
- Fenced lines wrap at the field's width with the words around them.
  `QPlainTextDocumentLayout` has one wrap mode for the whole document, so a
  single line cannot be left unwrapped. Wrapping matches GitHub's own comment
  box and the drawer rule. The exact lines, unwrapped and scrolling sideways
  in their own box, are the suggested change view under the field
  ([[BPR-2271]]).

## Acceptance

- AC1. A draft of prose, then a ```` ```suggestion ```` block, then prose:
  the field wraps at its width, has no sideways scroll and keeps its own
  font. The prose lines wrap and keep the field's font. Only the fence and
  code lines are in the code font, and the fences are quieter than the code.
- AC2. The same holds for ```` ```cpp ```` and for a ```` ~~~python ````
  block with no suggested change.
- AC3. Text typed after the block stays prose.
- AC4. A suggestion's caret is at the end of the block's last line, even
  with words after the block.
- AC5. `qt_conversation_cards_view` (with the existing suggestion
  assertion, which now expects a wrapping field), `qt_settings_window`,
  `architecture_boundaries`, `coding-format-check` and the documentation
  gate pass.

## Verification

Run on macOS, dev preset, in a task build tree. See the branch's commit for
the exact revision.

| Criterion | Check | Result |
|---|---|---|
| AC1–AC4 | `qt_conversation_cards_view`, `test_a_fenced_block_styles_only_its_own_lines` (suggestion, `cpp`, `~~~python`); renders through `--render-fenced-composer DIR` | see commit |
| AC5 | as listed | see commit |

## Remaining

- The owner's look at a frozen build: a draft with words, a suggestion
  block and words, and the same with a ```` ```cpp ```` block. One
  Accept/Reject question each. Offscreen renders substitute the code
  font's face, so only a native build shows the real look.
