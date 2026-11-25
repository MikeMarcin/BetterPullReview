---
schema: "implement-rfc.card.v1"
id: "BPR-2390"
title: "A report's findings donut and counts stand above its attachment in the Agent tab"
status: "Done"
rfcs: []
depends_on: ["BPR-2331", "BPR-2344"]
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"786900629","reservation":"docs/issues/_reservations/BPR-2390--BPR-2390--e7eb9b3ce2eb.json","origin":"owner's request with a screenshot of the Agent tab, 2026-11-25","related_cards":["BPR-2331","BPR-2336","BPR-2344","BPR-2166","BPR-2297"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"],"owner_step":"attended look at the review turn on nested-folders#21 in a frozen tests-on build (fixture agent), Light and Dark, at the narrowest drawer and a wide one"}
---

# BPR-2390: A report's findings donut and counts stand above its attachment in the Agent tab

## The owner's words

2026-11-25, with a screenshot of a review turn in the Agent tab (the
attachment row "Review of #21 / Report, 5 sections", under it the donut with
5 in its centre and the legend issue, question, nitpick, suggestion, praise,
then "Fixture agent answered in 0:00 · $0.00 · …"):

"I'm thinking that I want to move the report findings donut & counts above
the attachment."

## Before

[[BPR-2331]] put a report's tally under its attachment row in the Agent tab
and [[BPR-2344]] drew it as a donut with a legend. A report turn's reply read:
verdict, summary, attachment row, donut and legend, byline.

## Change

The reply reads: verdict, summary, donut and legend, attachment row, byline.
The counts read on from the summary they count, and the attachment, which
opens the report, closes the reply above the byline.

- **Order** (`agent_tab_view.cpp`, `agentTurnReportCard`): the tally is the
  card's first part and the attachment row its second. This is the one place
  the pair is built, so a live turn and a restored turn are the same.
  The report page, the tour chat and the item chat have no such pair.
- **Spacing**: the summary stands 5 under the answer and the attachment row
  10 under the taller of the donut and the legend
  (`report_tally_view::attachment_gap`). An attachment with no counts keeps
  its 9 under the answer. The card's spacing is set, not the style's: the
  platform's default differed by which control stood first, and under
  QMacStyle the plain swap made the reply 8 points taller.
  Measured on the native renders at 360: text to ring 18, ring to row 14, row
  to byline 11; the reply is 2 points taller than before.
- **Alignment**: the answer, the summary and the row share the reply's left
  edge; the row keeps the answer's right edge. The ring keeps its 3 point
  inset, the room its lifted slice needs.
- **Keyboard**: Tab goes through the counts in the report's order, then to
  the attachment. The legend rows are made when the counts arrive, and a
  widget joins the end of its window's focus order when it is made, so the
  rows stood after everything under the reply. `report_tally_view::order_rows`
  puts them where the legend stands.
- **In view at the turn's end** (`keep_report_in_view`): a report turn that
  ends with the conversation followed to its end comes back to show the
  summary and the attachment. Where the conversation is too short for both,
  the attachment is the one kept whole.
- **Found on the way**: a live report whose answer came whole while the turn
  ran, with no finding card, showed no summary until the turn was restored.
  `set_answer` returns early for an unchanged answer, and only
  `claim_findings` counted the tally at the end. The turn's end now counts it
  either way.

## Acceptance

- **AC1, order.** `qt_agent_report_summary_scale_100/125/200`
  (`test_the_summary_stands_over_its_attachment`), a restored review turn in a
  real window's Agent tab at 280, 320, 360, 420 and 640 drawer widths and
  with a 1.6 times larger font at 640 and 280: the summary is under the
  answer, the attachment under the summary, the byline under the attachment.
  Pass.
- **AC2, spacing.** Same test: the attachment is exactly `attachment_gap`
  under the taller of the donut and legend; the byline follows the attachment
  by the reply's own spacing; the summary stands nearer the answer than an
  attachment alone does and keeps room of its own; a report that labels no
  finding shows its attachment alone, 9 under its answer. Pass.
- **AC3, alignment.** Same test: answer, summary and attachment share the
  reply's left edge and the attachment the answer's right edge. The existing
  whole-summary checks (`expect_whole`: every row whole, beside the donut,
  inside the drawer, at the narrowest and a wide drawer and a larger font)
  still pass, and `qt_drawer_surfaces_fit_scale_100/125/200` pass. Pass.
- **AC4, keyboard.** Same test: the next Tab stop after each count is the next
  count, after the last count the attachment; Tab pressed on the last count
  focuses the attachment and Shift+Tab goes back. Pass.
- **AC5, in view.** `test_a_report_turn_ends_with_its_attachment_in_view`, a
  live turn ending under 900 points of later content: in a 620 window (a 272
  point conversation) the attachment is whole in view and the summary over it
  too; in the shortest window (360, an 85 point conversation) the attachment
  is whole at the foot. The live turn has no finding card and shows its
  summary, which is the defect found on the way. Pass.
- **AC6, the application.** `application_review_report_public_path` (the
  shipped composition on nested-folders#21, answered by the fixture agent):
  the summary ends at or above where the attachment starts. Against main's
  layout the same check fails with "the summary stands over the attachment:
  it ends at 171, the attachment starts at 9". Pass.
- **AC7, gates.** `architecture_boundaries` pass;
  `python3 scripts/verify_documentation.py --root .` DOCUMENTATION_OK;
  `format_check.py` on the eight changed C++ files pass; `tidy.py` on
  `agent_tab_view.cpp`, `report_tally_view.cpp`,
  `agent_report_summary_tests.cpp` and `review_report_public_path.cpp` pass.

Also run and passing with the change: `qt_report_attachment_title_scale_100/125/200`,
`qt_agent_tone_colours`, `qt_agent_footer`, `qt_agent_restored_byline`,
`qt_agent_finding_cards_retained`, `qt_agent_finding_cards_restart`,
`qt_explain_file_card_scale_100/150/200`, `application_follow_up_public_path`,
`application_ask_menu_public_path`, `application_ask_report_public_path`,
`application_explain_file_public_path`, `application_commit_asks_public_path`,
`application_report_draft_reveal_public_path`,
`application_agent_keep_action_public_path`.

Evidence revision: 786900629.

## Renders (not committed)

Before and after, from the application's own composed window: the public-path
fixture on nested-folders#21, Review this pull request answered by the fixture
agent, drawn through Cocoa with QMacStyle and windows kept off screen
(`BPR_NATIVE_LOOK=1`), in the theme the appearance setting follows
(`BPR_NATIVE_LOOK_SCHEME` light and dark), at `QT_SCALE_FACTOR` 1, 1.25 and 2
and drawer widths 280, 320, 360, 420 and 640. `review_report_public_path.cpp`
draws them under `BPR_REVIEW_REPORT_LOOK`: the reply, the drawer, and at 360
the whole window. Before is main's layout built with the same course.

## Left as found

- Under QMacStyle the legend's rows overlap: each is 22 points tall and they
  stand 14 apart, because the style gives a push button a layout rectangle
  smaller than the widget. The owner's screenshot and both sets of renders
  show it as the tight legend. `qt_agent_report_summary` run with
  `BPR_NATIVE_LOOK=1` fails on it at "the row is under the one before", on
  main as here; the registered rows run offscreen and pass. Not changed: it
  would change the legend the owner accepted in [[BPR-2344]].
- "Native continuation" stands at the drawer's left edge, without the
  conversation's inset, in both sets of renders.

## Owner look

Pending: whether the donut over the attachment reads right, in Light and
Dark, at the narrowest drawer and a wide one.
