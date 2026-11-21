---
schema: "implement-rfc.card.v1"
id: "BPR-2338"
title: "A jump in the report shows its item whole, a line below the tab strip"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2338--BPR-2338--14e19d24d65d.json","origin":"owner's direct look at main c27fe51fe, 2026-11-20, accepting BPR-2336 and BPR-2337 with one nit","related_cards":["BPR-2336","BPR-2331","BPR-2166"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2338: A jump in the report shows its item whole, a line below the tab strip

## The owner's words

Looking at main `c27fe51fe` on 2026-11-20, accepting [[BPR-2336]] and
[[BPR-2337]]:

"the scroll when clicking on something to jump to it isn't aligned
properly, the thing we're jumping to is cut off by the tab header."

Seen after pressing the Agent tab summary's "suggestion" segment: the report
scrolled so the finding's top, its label line, stood under the document tab
strip ("Review of #21"), cut off.

## Cause

Every jump in the report (an Agent tab summary segment or label, a section
of the report's menu, the attended fixture check's menu walk) goes through
`agent_report_view::go_to_heading`. It scrolled the heading's own text
line to 8 pixels under the pane's top. The tab strip does not overlay the
report: it is laid out above it in the comparison page's column. But a
finding's heading is not its top. In the page a finding is a box: its rule
and border, 10 pixels of padding, then its label line ("suggestion") and
place, and only then its heading. A section's name sits under its rule and
14 pixels of padding. Bringing the heading to the top left the box's
border, padding and label line above the pane's top edge, so they looked cut
off by the tab strip right above it. Measured in the new course on the old
code at 1.0: the report's top at 106, the box at 85, the label line at 96,
the heading at 114.

A second, smaller cause showed in the fixture agent's course: on a freshly
drawn page, measuring the heading can lay the page out further, and what
stands above the page moved by 19 pixels after the scroll was set. The
jump then landed short.

## Change

- `heading_offset` (and so `section_top`, `current_section` and
  `go_to_heading`) measures an item's top as the top of the outermost box
  its heading is in (the frame's bounding rectangle, which starts at its
  border, under its margin), or the heading's line when it is in no box.
- The jump leaves one line of the page's font (`jump_margin()`, the shown
  text's line spacing) between the pane's top and the item, in place of the
  8 pixels.
- `go_to_heading` aims again (at most four passes) until the item holds
  still where it was aimed: settled, measured, the room under the last
  section made if needed, scrolled, and measured once more.
- `src/app/main.cpp`'s attended fixture check of the report menu expects a
  section between one and one-plus-12 pixels of a line below the pane's top.
  The composition contract's production app manifest digest is resealed
  for that line.

Other jump-to surfaces were checked for the same fault. None uses this
helper or scrolls under an overlaying header: the tour agent panel's
transcript scrolls a run's anchor block to the top of its own
`QTextBrowser`; a reader's in-place link uses `ensureVisible` with room
around it; the conversation cards view keeps a post in place across a
relayout. None needed a change.

## Acceptance

- AC1. After any jump in the report (each summary band, each section of the
  menu), the jumped-to item's top (its box's border, above its label line)
  stands at least one line below the document tab strip's bottom and the
  pane's top, and no more than two pixels further down; its first line and
  heading are below that top.
- AC2. The tab strip does not overlay the report.
- AC3. The menu still marks the section jumped to; the fixture agent's
  attended course walks every report's menu.
- AC4. `architecture_boundaries`, the format check and the documentation
  gate pass.

## Verification

macOS arm64, dev preset (Debug with `-gline-tables-only`), the lane's
`build/dev` on main `b5ab061bc` plus this change. `format_check.py`
(clang-format 22.1.8) on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2, AC3 | `qt_agent_report_summary_scale_100/125/200` › `test_a_jump_shows_its_item_whole_under_the_tab_header` (every summary band, the suggestion first, then every menu section from the last up; the item's box top against the tab strip's bottom plus a line, the report's top, the label line and heading, `section_top`, `current_section`) | pass |
| Reproduced | The same course on the old `agent_report_view.cpp`: "the item's top stands a line below the header: box 85, label 96, heading 114, header's bottom 106" at 1.0 and 2.0 | fail, as the owner saw |
| AC3 | `application_fixture_agent_every_ask` (the attended fixture check's report menu walk). It failed on the first version of this change, before the re-aim, at "section=4 top=34" | pass |
| Not broken | `qt_agent_report_view`, `qt_agent_tone_colours`, `qt_agent_restored_byline`, `qt_side_chat_tab_and_send_scale_100/200`, `qt_markdown_code_1_0/2_0`, `qt_nested_scroll_latch`, `application_review_report_public_path`, `application_follow_up_public_path` | pass |
| Composition | `application_review_round_trip_composition_contract`: the resealed manifest digest (`1ae00611…`) matches; the course then stops at `SEMANTIC_COMMENT_PROBE_FAILED phase=4`, which [[BPR-2269]], [[BPR-2322]], [[BPR-2329]] and [[BPR-2333]] record as predating them | digest pass; known stop |
| AC4 | `architecture_boundaries`, `format_check.py`, `tidy.py` on `agent_report_view.cpp` (no finding on a changed line), `python3 scripts/verify_documentation.py --root .` (`DOCUMENTATION_OK`) | pass |

No owner look was prepared in this round; the owner's judgement of the new
alignment is not recorded here. Not run: Windows and Linux, a full `ctest`.
