---
schema: "implement-rfc.card.v1"
id: "BPR-2231"
title: "A report's Drafted and Kept marks are stored and shared with the Agent tab's finding cards"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
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
