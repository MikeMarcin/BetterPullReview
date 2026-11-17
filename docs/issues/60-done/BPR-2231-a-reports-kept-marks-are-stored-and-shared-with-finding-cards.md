---
schema: "implement-rfc.card.v1"
id: "BPR-2231"
title: "A report's Drafted and Kept marks are stored and shared with the Agent tab's finding cards"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
acceptance: [{"criterion": "AC-a", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "core_agent_report (one memory adopted after a restart) and application_review_report_public_path (composed, fixture agent: after a restart on the same profile both cards read Drafted and every finding and the summary still read Drafted or Kept)"}}, {"criterion": "AC-b", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "application_review_report_public_path: Draft comment from the report makes the card read Drafted; the card's own Draft makes the report's finding read Drafted and not draftable twice; qt_agent_report_view; owner Accept report-marks-shared (session 0be6df2e1b895302d0be6058915b8289, decision b64c4fd5)"}}, {"criterion": "AC-c", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "core agent_report_keep (report_finding::Card, report_finding_key_of, report_keep_record, report_finding_standing_of) over the one agent_finding_card_memory the host stores; the report view keeps only what is under way; architecture_boundaries and architecture_boundary_contract pass"}}]
---

# A report's Drafted and Kept marks are stored and shared with the Agent tab's finding cards

## Origin

What a report's keep action made (Drafted, Kept) is remembered by the report view for the session, by the report's turn ([[BPR-2219]]). It is lost at restart, and the Agent tab's finding card for the same finding does not know of it, so one finding can be drafted twice from two places.

## Scope and acceptance

- **AC-a.** A finding drafted or kept from the report still reads so after a restart.
- **AC-b.** A finding drafted from the report reads Drafted on its Agent tab card, and the other way round.
- **AC-c.** The state is the turn's one state for that finding, decided in core, not kept twice.

## Built

Built with [[BPR-2232]], one change. `frontend-design:frontend-design` was
loaded before the Qt work.

- **Core** (`agent_report_keep.hpp`, `report_findings`): a report finding at
  a finding card's place is that card (`report_finding::Card`; every card of
  the turn is matched by place, one with the agent's comment first). Its one
  state is kept where the card's is: under the card's findings block and
  ordinal. A finding no card carries, and the summary, are kept under the
  report's own digest (`digest_report_findings`, a prefix no findings block
  can have) and its row. `report_keep_record` is what a finished keep
  leaves; `report_finding_standing_of` is what the report says of a kept
  state. Both live in the one `agent_finding_card_memory` the host already
  stores, so nothing is kept twice.
- **Host** (`window_quick_ask.cpp`, `window_agent_setup.cpp`): the report
  reads each state from that memory; a report keep on a card's finding goes
  through the card's delivery and finding, and the card is told its state;
  every finished keep is remembered (`settle_report_finding`) and written by
  the existing store. A card's own Draft, Undo of a Draft all, and the
  memory read at start tell the report (`settle_keep`, `refresh_keeps`).
- **Qt** (`agent_report_view`): the view keeps only what is under way; what
  was made is read from the host's records.
- **Found while rendering the look** (`9ae980844`): a report opened before
  the scroll area gave its column a width was laid out at 29 pixels, and its
  code boxes stayed one pixel wide until the page was drawn again (since
  [[BPR-2238]] reads the box's width from the cell). The column's resize now
  lays the page out; the course asserts the boxes' width at first open and
  after the restart.

## Verification

On the lane at `9ae980844` (dev preset, macOS), with [[BPR-2232]]:

- `core_agent_report` (a finding is the card at its place, a card with the
  agent's comment first; keys; one memory across a restart; Draft all's
  candidates), `core_agent_draft_all`, `core_agent_finding_card_memory`.
- `qt_agent_report_view` (records read from the host; a card's Drafted on the
  report; Draft all offered, under way, settled, said; a refusal said at its
  finding and offered again).
- `application_review_report_public_path` (composed, fixture agent): Draft
  comment from the report makes the issue's Agent tab card read Drafted; the
  tonemap card's own Draft makes the report read Drafted there and not draft
  it twice; Draft all drafts the other two and says "Drafted 2 private drafts.
  Skipped 2: 2 already drafted."; the report is in front at the end; nothing
  is published; after a restart on the same profile both cards read Drafted
  and every finding and the summary still read Drafted or Kept.
- `application_ask_report_public_path`, `application_ask_menu_public_path`,
  `application_agent_keep_action_public_path`,
  `qt_agent_finding_cards_retained`, `qt_agent_finding_cards_restart`,
  `architecture_boundaries`, `architecture_boundary_contract`, the
  documentation gate.
- Not run: the whole suite; `coding-tidy`.

## Owner look (filed, not waited on)

Attended-review session `0be6df2e1b895302d0be6058915b8289`, external state
directory `~/.attended-review-state/report-keeps-v001-2026-11-16/` (`state/`
the session, `manifest.json`, `serve.log`, `serve.pid`), served on the
tailnet. Two Accept/Reject questions on the frozen build `report-keeps-v001`
(root `~/report-keeps-v001.0rwtd9d6`, frozen from `9ae9808447d8`, Fixture
agent only, fixture `returning-review-v1`): `report-draft-all` ([[BPR-2232]])
and `report-marks-shared` ([[BPR-2231]]). Its images are the composed
course's renders of the same commit; the restart is the course's, since each
launch of the frozen build starts a fresh profile. Lifecycle: handed back. On the
verdicts: stop the host by its pid, remove the root with
`scripts/freeze_attended_build.py remove`.

### Verdict, look report-keeps-v001 (handed back 2026-11-16), and close

- **Accept** `report-marks-shared` (decision `b64c4fd5`), verbatim: "Minor
  nit: it would be nice if it remembered you were on the agents tab when
  swapping between overview and review (eg by opening the report on
  review)." The owner clarified: "Specifically I mean when you go to review
  still have the agent tab open." Fixed at `b18374f7f`: if Overview's Agent
  tab is open when the reviewer moves to Review (including by opening a
  report), Review's drawer opens on the Agent tab. This holds within one
  pull request; a different pull request still opens fresh ([[BPR-2185]]).
  `qt_workbench_window_scale_100` checks it.
  `application_ask_*` and `application_returning_review_addressed_public_path`,
  which opened the Agent tab on Overview and then used Comments in Review,
  now choose Comments. The ask-report course's "as tall as its content"
  check measured the hidden body. It passed only while the page was laid out
  at a narrow column. It now measures the reader that is shown.

Closed 2026-11-16. The look's host is stopped and its frozen root removed.
