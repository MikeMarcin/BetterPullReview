---
schema: "implement-rfc.card.v1"
id: "BPR-2232"
title: "Draft all from the report"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# Draft all from the report

## Origin

The report offers one keep action a finding ([[BPR-2219]]). The Agent tab's Draft all still drafts every finding card of an answer, but the report the reviewer reads has no way to draft the rest at once (design round review-report-v1, B's band).

## Scope and acceptance

- **AC-a.** The report offers drafting every finding not yet drafted, in the Agent tab's Draft all order and by its path.
- **AC-b.** It says how many were drafted and which could not be, as Draft all does.
- **AC-c.** Nothing is published.

## Built

Built with [[BPR-2231]], one change. `frontend-design:frontend-design` was
loaded before the Qt work.

- **Core** (`report_draft_all_candidates`): the report's findings as the
  Agent tab's Draft all reads its cards, over their one kept state
  ([[BPR-2231]]); `plan_draft_all` and `draft_all_offer`'s rule (two
  suggested comments or more, one to draft) decide, in the report's order.
- **Host**: the report's run is `draft_all_agent_cards`, the Agent tab's
  path, each finding through `act_on_agent_card`; each settles into the kept
  state as it is made. At the end the report is brought back to the front,
  not the Agent tab, and the batch is not the Agent tab's to undo.
- **Qt**: in the band under the summary, below Keep summary, **Draft all
  (N)** with "Each finding not yet drafted, at its lines. Nothing is
  published"; while it runs, "Drafting N findings…" and each finding reads
  Drafting…; then the sentence Draft all says ("Drafted 2 private drafts.
  Skipped 2: 2 already drafted."), a refusal named at its finding.
