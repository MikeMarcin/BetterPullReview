---
schema: "implement-rfc.card.v1"
id: "BPR-2353"
title: "A suggestion block turns the whole comment composer into unwrapped code"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","target":"0.5.0","origin":"owner, 2026-11-21","related_cards":["BPR-2271","BPR-2273","BPR-1590","BPR-2352"],"related_rfcs":["RFC-BPR-0090"],"acceptance":["AC1","AC2","AC3","AC4","AC5"],"provisional_id":"BPR-XXXX-B","branch":"claude/suggestion-guidance-and-composer","reservation":"docs/issues/_reservations/BPR-2352--BPR-2355--081e6197acbb.json","attended_review":[{"session":"4708604404524750554d23a4282d7890","decision":"accepted","date":"2026-11-21"}]}
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
- Superseded in part by [[BPR-2354]] (owner, 2026-11-21: "Can we not do
  the nested code block element like we do for agent replies inside the
  composer?"): a code-aware layout now keeps a block's lines unwrapped in
  their own box with their own sideways scroll, while the words still wrap.

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

## Owner acceptance

2026-11-21, session `4708604404524750554d23a4282d7890`: the wrapping
composer with a fenced block, **Accept**. The native look the offscreen
renders could not show was judged in that frozen build.

## Landing evidence (main, macOS, Debug)

- Allocated on main `084b8ff88` (`BPR-2352`..`BPR-2355`). The branch
  `claude/suggestion-guidance-and-composer` was rebased onto it and each
  commit rewritten so the provisional `BPR-XXXX-A`, `-B`, `-D` and `-E`
  became `BPR-2352`, `BPR-2353`, `BPR-2354` and `BPR-2355` in the cards,
  code and test comments, docs and commit subjects. The review-only fixture
  commits of `claude/review-composer-2` were not brought.
- Merge with [[BPR-2351]]: a report's suggested comment draws its fenced
  blocks through `code_box_html`, the page's own plain HTML box that the owner
  accepted, never a collected code view. It needs no anchor, because page
  HTML is not coloured. `application_fixture_agent_every_ask` counts the
  report's dressed blocks against its code views, and
  `qt_agent_report_view` now expects the item's two views plus the fix
  shown in the page.
- The production app manifest digest needed no reseal:
  `application_review_round_trip_composition_contract` passed over the
  merged tree as it stood.
- Full dev build in the lane tree succeeded. Passed:
  `application_review_round_trip_composition_contract`,
  `application_agent_invocation_composition_contract` (its review skill pin
  moved to version 7 with this change), `tour_cli_composition`,
  `tour_cli_process_commands` and `skill_pr_tour_package` (in a side tree
  configured with `BPR_BUILD_TOUR_CLI=ON`), `qt_conversation_cards_view`,
  `qt_agent_report_view`, `core_agent_report`, `core_review_skill`,
  `application_writing_delivery`, `application_review_skill_host`,
  `application_agent_launch_policy`, `qt_settings_window`,
  `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`,
  `qt_item_side_chat`, `qt_tour_side_chat`,
  `application_review_report_public_path`,
  `application_follow_up_public_path`,
  `application_fixture_agent_every_ask` and `architecture_boundaries`.
  `coding-format-check` and the documentation gate passed.
