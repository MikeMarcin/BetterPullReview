---
schema: "implement-rfc.card.v1"
id: "BPR-2354"
title: "A fenced block in the comment composer is its own code box: unwrapped, scrolling sideways on its own, while the words wrap"
status: "Done"
rfcs: []
depends_on: ["BPR-2353"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner, 2026-11-21, pushback on BPR-2353","related_cards":["BPR-2353","BPR-2271","BPR-2273","BPR-1083","BPR-2322","BPR-2298"],"related_rfcs":["RFC-BPR-0090"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"provisional_id":"BPR-XXXX-D","branch":"claude/suggestion-guidance-and-composer","reservation":"docs/issues/_reservations/BPR-2352--BPR-2355--081e6197acbb.json","attended_review":[{"session":"4708604404524750554d23a4282d7890","candidate":"layout","decision":"accepted","date":"2026-11-21"},{"session":"4708604404524750554d23a4282d7890","candidate":"sideways scroll","decision":"accepted with a swipe note, fixed","date":"2026-11-21"},{"session":"772f868fd02ed420c8889a2d055f5fab","candidate":"trackpad swipe","decision":"accepted","date":"2026-11-22"}]}
---

# BPR-2354: A fenced block in the comment composer is its own code box

## The owner's words

2026-11-21, on [[BPR-2353]]'s note that the composer can only wrap the
whole field or none of it: "Can we not do the nested code block element like
we do for agent replies inside the composer?"

## Options weighed

- **(a) A composer made of segments**: prose fields with embedded code
  editors. Rejected. The composer is one `QPlainTextEdit` to everything that
  writes in it: about 220 call sites through `draft_editor()` and
  `reply_editor()`, the label controls ([[BPR-1083]]), Enhance, Ask about
  this's Update draft ([[BPR-2322]]), the range promotion ([[BPR-2298]]) and
  the composed courses. Segments would also need hand-made caret movement,
  selection, copy, undo and input-method handling across their seams.
- **(b) A `QTextEdit` with non-breakable lines.** Rejected. An unwrapped line
  widens the whole document, so the whole field would scroll sideways, which
  no drawer may do.
- **(c) Chosen: a code-aware layout for the same field.**
  `QPlainTextEdit::setDocument` takes any `QPlainTextDocumentLayout`. The
  new `composer_code_layout` subclasses it and overrides
  `blockBoundingRect` and `documentChanged`, so it lays out a fenced block's
  lines itself.

## Change

- `composer_code_layout` (`src/adapters/qt/src/`) leaves the words to the
  plain layout, wrapped at the field's width. It lays out each line of a
  fenced block (any language, `suggestion` included, as
  `fenced_code_highlighter` reads it) unwrapped. A block scrolls sideways by
  moving its code lines' x position. Drawing, hit testing, the caret and
  the selection all read the same lines, so they agree. The fences keep
  their place as the box's header and footer rows. The field's own sideways
  range stays zero.
- The field (`reply_text`, the comment composer and the reply field) paints
  each block as the agent reply's code box (`markdown_code_block`):
  - the palette's window colour as its ground, a 6-pixel radius and a
    hairline outline;
  - the opening fence as the header row, a shade toward the ink, over a
    hairline;
  - the code 10 pixels in from the sides, its scrolled-off ends masked at
    the box's edges.
- A block wider than its box has its own horizontal `QScrollBar`
  (`threadCodeBlockScroll`) under its last code line. A sideways wheel or
  trackpad over the block moves the block, and a vertical one is the
  field's, then the drawer's, as before. The caret in code keeps the block
  scrolled so the caret shows.
- One plain text document throughout. Undo and redo, copy and paste,
  input methods, accessibility, the label controls, Enhance, Update draft and
  the BPR-2298 range promotion all work on the same `QPlainTextEdit` and
  its text, which is the draft's exactly, fences included.

### Decisions

- The frontend-design skill was loaded. The box takes the agent reply's
  code box's measures and colours, so a block reads the same when written
  and when read. The fence text stays visible in the header row: it is the
  draft's text and can be edited there.
- Syntax colouring is not in this change. The reader colours code through
  the application's syntax service, asked per block and answered later.
  Asking it on each keystroke in the composer is a separate wiring of the
  card's syntax dispatch, left for a follow-up. The code is in the code font
  with muted fences ([[BPR-2353]]).
- The box's paddings count in the field's height (`fit`). The field's
  vertical scroll still steps by lines, as a plain field does, so a step
  may stop a few pixels inside a padded row.

## Owner review (2026-11-21)

In session `4708604404524750554d23a4282d7890` (this branch merged with
`claude/context-aware-code-highlighting`) the box layout and the sideways
scroll were both **Accepted**, with one note: "my sideways scrolling
gestures are not scrolling it."

- **Cause.** The drawer's gesture latch, `nested_scroll_latch`
  ([[BPR-2279]], [[BPR-2292]]), offers a trackpad gesture's first movement
  only to a scroll area whose own bar can move that way. The comment
  field's sideways range is always zero by design, so the field was passed
  over and the drawer took every swipe. A mouse's sideways notches, which
  have no phase, did reach the box.
- **Fix.** `nested_scroll_latch::scrolls_itself_property`
  (`bprScrollsItself`). A scroll area that moves part of itself is offered
  the first movement whatever its bars say. It owns the gesture only if it
  takes that movement, so a box at its end is passed over. The comment
  field sets it. Its `wheelEvent` keeps a gesture on the block it began
  over, momentum included, and accepts a movement only when the block
  moved.
- **Test.** `test_a_code_block_is_its_own_box` sends swipes through the
  platform (`QWindowSystemInterface`: ScrollBegin, ScrollUpdate with
  pixels, ScrollMomentum, ScrollEnd), so the latch takes part. The box
  scrolls and its momentum carries on, while the field and the drawer stay
  put. A swipe back scrolls it back, and a mouse's notch scrolls it. Run
  without the property, the same test fails as the owner saw it: the swipe
  moved the box 0 of 383 pixels.
- Syntax colouring and the language picker are [[BPR-2355]].

## Acceptance

- AC1. The words before and after a fenced block wrap. The block's long
  line is one line, wider than the field.
- AC2. The block has its own sideways bar. Moving it moves every code line
  of the block together and nothing else. The field's sideways range stays
  zero.
- AC3. A click in a scrolled block lands on the character shown. The caret
  moves from the words through the fence into the code. At the end of a long
  line the block, not the field, scrolls to show the caret.
- AC4. A selection crosses from the words into the block.
- AC5. The field's text is the draft's exactly. An edit in the block is
  undone like any other. A block typed into the field is a box at once, and
  undo takes it back whole.
- AC6. `qt_conversation_cards_view` (with [[BPR-2353]]'s fenced-line test
  and the suggested change box, [[BPR-2271]], and long draft, [[BPR-2273]],
  courses), `architecture_boundaries`, `coding-format-check` and the
  documentation gate pass.

## Verification

Run on macOS, dev preset, in a task build tree. See the branch's commit for
the exact revision.

| Criterion | Check | Result |
|---|---|---|
| AC1–AC5 | `qt_conversation_cards_view`, `test_a_code_block_is_its_own_box`; renders through `--render-code-box DIR` | see commit |
| AC6 | as listed | see commit |

## Owner acceptance

- 2026-11-21, session `4708604404524750554d23a4282d7890`: the box layout,
  **Accept**; the sideways scroll, **Accept**, with the swipe note fixed
  above.
- 2026-11-22, session `772f868fd02ed420c8889a2d055f5fab`: a trackpad swipe
  scrolls the box, **Accept**.
- Syntax colouring in the composer's code box is [[BPR-2355]].

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
