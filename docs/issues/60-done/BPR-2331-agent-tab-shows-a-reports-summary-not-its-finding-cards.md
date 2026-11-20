---
schema: "implement-rfc.card.v1"
id: "BPR-2331"
title: "The Agent tab shows a report's summary, not its finding cards"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2331--BPR-2331--4d6e7f490adf.json","origin":"owner, 2026-11-20","related_cards":["BPR-2296","BPR-2297","BPR-2322","BPR-2325","BPR-2231","BPR-2232","BPR-2061"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"]}
---

# BPR-2331: The Agent tab shows a report's summary, not its finding cards

## Owner (2026-11-20)

"with auto scroll now it becomes obvious that when there are suggestions that
draw after an attachment it scrolls too far down and you cant even see the
attachment and have to scroll back up. I think the problem here is we have
these suggestions duplicated in the agent tab and report, let's just keep in
the report when they're part of the report and remove them from the agent's
tab. We can put a brief summary in the agents tab below the report. The line
at the top of the report is a good candidate for that although we'd need a
version that works in the narrow agent drawer, perhaps it gets transposed."

## Cause

A review turn shows its report attachment ([[BPR-2166]]) and then, under it,
one finding card per finding of the answer's findings block ([[BPR-1753]]).
The report shows the same findings with the same actions ([[BPR-2219]],
[[BPR-2231]]). With stick-to-bottom ([[BPR-2296]]) the conversation follows
the cards to their end, so the attachment above them scrolls out of view.

## Change

- A finding card the turn's report holds (a report finding at the card's
  place, `core::report_findings`' `Card`) is not shown in the Agent tab. Its
  card entry stays: it carries the finding's one state, which the report
  reads and the host keeps ([[BPR-2231]], [[BPR-2325]]). A card no report
  finding holds, and every card of a turn that is not a report, shows as
  before.
- Under the attachment, the report's tally (`core::dressed_report::Tally`,
  the line at the top of the report page) transposed for the drawer: one row
  a label, its word in its tone, a bar as long as its share of the largest
  count, and the count. A row opens the report at the first finding it
  counts.
- The Agent tab's Draft all row is not shown on a report turn; the report's
  own Draft all ([[BPR-2232]]) is the one.
- What only the card offered moves to the report finding: Dismiss and its
  Undo, the suggested comment the agent wrote for it, and what Drafted says
  about the lines it took.
- A turn that ends with a report, at the conversation's end, ends with its
  attachment in view.

## Acceptance

- AC1. Core: each tally count names the menu row of the first item it
  counts; which of a turn's finding cards its report holds is the core's.
- AC2. Qt: a report turn whose report holds its findings shows the
  attachment and the summary and no finding cards; a card its report does
  not hold still shows; a turn that is not a report keeps its cards.
- AC3. Qt: the summary rows are whole (word, bar and count inside the row,
  nothing clipped) at the drawer's narrowest width and a wide one, at 1.0,
  1.25 and 2.0 scale.
- AC4. Qt: pressing a summary row opens the report at its label's first
  finding.
- AC5. Qt: a report turn that ends at the conversation's end leaves its
  attachment in view.
- AC6. Qt: the report offers Draft comment, Dismiss (and Undo), Ask
  follow-up and, after its draft is deleted, Draft comment again; the
  suggested comment shows on its finding.
- AC7. `architecture_boundaries`, `coding-format-check` and
  `python3 scripts/verify_documentation.py --root .` pass.

## Decisions

- The frontend-design skill was loaded for the summary.
- The summary's design: the report page's meter turned on its side. One
  row a tally count, hung from the attachment by a thin rule that drops
  from under the attachment's page glyph with a short tick to each row.
  Three columns measured from the current font: the word in its tone (the
  theme's `report_verdict_*` roles, neutral in the window's ink), a 4 point
  bar in the same tone over a faint track, as long as its share of the
  largest count, and the count in semibold, right-aligned. No pluralising:
  the count stands in its own column. A row is a button: hover wash, focus
  outline, tooltip "Open the report at its first issue", accessible name
  "issue: 2". A row never asks for less than its word, an 8 point track and
  its count, so the drawer never pans sideways at its 280 point minimum.
  `bpr::qt::report_tally_view` (`src/adapters/qt/include/bpr/qt/report_tally_view.hpp`).
- Which cards a report holds is `core::report_findings`' `Card` mapping,
  through `core::report_held_cards`; every card stays a card entry, because
  the entries carry each finding's one state ([[BPR-2231]], [[BPR-2325]]).
  Nothing in the card code became unused, so no card code was removed: a
  turn that is not a report, and a card no report finding is at, still show
  cards.
- A tally row opens the report at its count's first item's menu row
  (`dressed_count::First`, `agent_report_document::Heading`, the report
  view's `go_to_heading`). The workbench does not keep the heading with the
  report, so a later re-presentation (Draft all's return) does not jump.
- What only the card had, moved to the report: Dismiss and Undo (a card's
  through its card, a finding no card carries under the report's own
  record), the agent's suggested comment ("Suggested comment:", or
  "Suggested comment, updated:" after a follow-up), and what Drafted says
  of its lines ([[BPR-2298]], kept by the view, since a record keeps a
  reason only with a refusal). The Agent tab's Draft all row, with its
  Undo, is hidden on a report turn that holds cards; the report's Draft all
  is now undoable from the report (the host keeps its batch).
- The attachment in view: when a report turn ends and the conversation is
  pinned to its end, the conversation is laid out and, if its end stands
  past the attachment, scrolled back to 12 points above the attachment
  (which unpins it, as any move up does).
- The 5 point sideways pan of the conversation at the drawer's narrowest
  width is not this change: with the summary hidden, and with each card
  hidden, the conversation's content still needs 267 points of a 262
  point viewport (69 points more at a 1.6 times font); its turns need it.
  The summary's own test checks what the summary needs, and checks each
  count is in view wherever the conversation does not already pan.

## Verification

macOS arm64, dev preset (Debug), a targeted build in the task's own tree
of `4102a9fbb` (this change on main `c9637b1ff`; built and run as
`581d542f0`, the same sources before the commits were squashed).
`coding-format` on every changed C++ file; clang-tidy clean on
`report_tally_view.cpp` (its four findings in `agent_report.cpp` are
pre-existing lines 871 and 872, `read_report_answer`).

| Criterion | Check | Result |
|---|---|---|
| AC1 | `core_agent_report` (each tally count names its first item's row, for labels and areas; the report holds only the cards its findings are) | pass |
| AC2 | `qt_agent_report_summary_scale_100/125/200` (a restored review: attachment and summary shown, the two cards at its findings' places hidden, the card elsewhere and a non-report turn's card shown, the Agent tab's Draft all row hidden); `application_review_report_public_path` and `application_follow_up_public_path` (the fixture agent's review: no card shown, the summary shown) | pass |
| AC3 | `qt_agent_report_summary_scale_100/125/200` (every row's word, bar and count inside it, apart and whole; the summary never needs more than the drawer gives; at 280 and 640 points, and at a 1.6 times font) | pass |
| AC4 | `qt_agent_report_summary_scale_*` (a row opens the report at its first finding's row, `section_top` within 24 points; another row moves the open report; the attachment opens it at no row) | pass |
| AC5 | `qt_agent_report_summary_scale_*` (a live report turn with 900 points under its reply ends with the attachment in view); without the fix it fails: "its top at -778 of 272" | pass; fails without the fix |
| AC6 | `qt_agent_report_view` (Suggested comment shown; Dismiss through a card's record and a report-own record, Undo; no keep while dismissed; Drafted says its lines; Draft all's Undo removes and says what it kept; [[BPR-2325]]'s Draft comment again after the draft is deleted); `application_follow_up_public_path` (the report's Ask follow-up, two rounds, Update suggested comment and Use shown on the report, the same chat reopened, Draft comment drafts it, deleted and drafted again) | pass |
| AC7 | `architecture_boundaries`, `coding_format_check` on the changed files, `verify_documentation.py` | pass |
| Not broken | `qt_agent_tone_colours` (now also the summary's issue row in Needs work), `qt_report_attachment_title_scale_100/125/200`, `qt_stick_to_bottom`, `qt_agent_finding_cards_retained`, `qt_agent_finding_cards_restart`, `application_agent_keep_action_public_path`, `application_agent_suggestion_range_public_path`, `qt_workbench_window_scale_100` | pass |

Looks: offscreen renders of the reply at 280 and 640 points, viewed and
discarded (the test saves them with `BPR_REPORT_SUMMARY_RENDER=<folder>`).
No attended look was prepared; no real agent, no live GitHub.

Not this change:
- `application_review_round_trip_composition_contract` was not run. Its
  production app manifest pin is `006099bb…`; main at `c9637b1ff` already
  reads `1a1d87dd…` (unsealed since [[BPR-2329]], whose reseal it is, as
  [[BPR-2333]] recorded). This change (`window_quick_ask.cpp`,
  `window_agent_setup.cpp`) moves it to `ebebaa25f52c40f16314196728821f9d89d626da75e2f955774a5ae4d3ecebf5`:
  the report's Dismiss goes through the Agent tab card's existing action or
  the finding card store; a report's Draft all keeps its batch for the
  report's Undo, through the existing Undo path. No grant, target,
  provider, credential, transport, GitHub write, mutation or publication
  path changes. The reseal belongs with BPR-2329's.
- `live_second_account_course` (live GitHub) was not run; it only counts
  the Agent tab's cards in its JSON.
