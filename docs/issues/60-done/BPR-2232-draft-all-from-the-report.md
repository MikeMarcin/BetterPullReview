---
schema: "implement-rfc.card.v1"
id: "BPR-2232"
title: "Draft all from the report"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
acceptance: [{"criterion": "AC-a", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "core_agent_report (report_draft_all_candidates, plan_draft_all in the report's order), qt_agent_report_view, application_review_report_public_path (composed, fixture agent: Draft all from the report drafts the two findings not yet drafted through the Agent tab's Draft all path); owner Accept report-draft-all (session 0be6df2e1b895302d0be6058915b8289, decision 1a82eeb3)"}}, {"criterion": "AC-b", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "qt_agent_report_view and application_review_report_public_path: 'Drafted 2 private drafts. Skipped 2: 2 already drafted.'; a refusal said at its finding; owner Accept report-draft-all (decision 1a82eeb3)"}}, {"criterion": "AC-c", "outcome": "pass", "revision": "b18374f7f", "evidence": {"kind": "command", "detail": "application_review_report_public_path: review mutation evidence unchanged after Draft all (no submission, stored comment or post)"}}]
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

## Verification

As [[BPR-2231]]'s (one change, `fe23e6040` and `9ae980844`): `core_agent_report`
(Draft all's candidates and plan over the report's states),
`qt_agent_report_view`, `application_review_report_public_path` (Draft all
from the report: two drafted in the report's order, two already drafted,
the sentence, the report in front, nothing published).

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

- **Accept** `report-draft-all` (decision `1a82eeb3`), verbatim: "Minor nit
  th report glyph is unreadable on dark theme, i dont need a rereview for
  that fix, just fix it." Fixed at `0307df05b`: the report tab's page glyph
  was drawn once in the tab bar's ink, and on macOS the style gives the tab
  bar its own palette, which a theme change does not reach. It is now drawn
  in the strip's ink and redrawn on every palette or theme change. It is the
  only glyph a document tab has (a file's tab has none).
  `qt_document_tab_glyph` checks light to dark and back, offscreen and
  native light and dark.

Closed 2026-11-16. The look's host is stopped and its frozen root removed.
