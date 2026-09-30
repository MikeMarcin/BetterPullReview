---
schema: "implement-rfc.card.v1"
id: "BPR-1242"
title: "The Notes layout rows predate the Private history page and inspect the hidden Agent tools page"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"recorded as pre-existing under BPR-1157 (RFC-BPR-0053-0061 ledger); filed by the main stabilization milestone on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1157","BPR-0585","BPR-1134"]}
acceptance: [{"criterion":"The six rows pass on macOS","outcome":"pass","revision":"577fd7fa5","evidence":{"kind":"command","detail":"ctest -R ^qt_(composed_github|document_tabs|workbench_focus_order|workbench_window)_scale_(100|125|200)$ -> 12/12 pass on formatted bytes; qt_composed_home_refusal_scale_* and tour editor rows also pass (15/15)."}},{"criterion":"Documentation gate","outcome":"pass","revision":"577fd7fa5","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1242: The Notes layout rows predate the Private history page and inspect the hidden Agent tools page

## Observation

At `main` e9e65eab2 on macOS arm64, `qt_composed_github_scale_{100,125,200}`
fail at `the agent launcher, the side chat, and the finding queue stack down one
visible column`, and `qt_workbench_window_scale_{100,125,200}` at `no Notes tab
control is clipped at 200 pixels; the column scrolls` (the helper reports `the
content is wider than the scroll range reaches`). All six passed on
2026-09-26 at beddf1f55.

17e6f85d1 ([[BPR-0585]], RFC-BPR-0023, closed and owner-accepted) gave the
Notes tray tab two pages in a `privateConversationTabs` tab widget: Private
history first, and Agent tools, the old Notes column, second. The rows still
treat the Notes tab as that one column. Choosing Notes now shows Private
history, so the agent surfaces are hidden (the composed row), and the
reachability helper measures the hidden Agent tools column, whose geometry is
never laid out at 200 pixels (the window row).

## Outcome

The rows follow the accepted arrangement: a course that inspects the agent
surfaces opens the Agent tools page as a reviewer would, and the 200-pixel
reachability check covers both Notes pages. No product layout changes.

## Direct acceptance

- The six rows pass on macOS.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (macOS arm64, 2026-09-29)

`577fd7fa5`. Each test executable stops at its first failed expectation, so
fixing the first stale one uncovered the next:

| Row | Stale expectation | Now |
|---|---|---|
| `qt_composed_github_scale_*` | agent surfaces visible after choosing Notes (three courses) | `show_agent_tools` opens the Agent tools page first |
| `qt_composed_github_scale_*` | a first load opens the provisional Overview | [[BPR-1200]] (3bbc6561c, parked for the owner) keeps Home and says "Loading the pull request." under the bar; the course asserts that, matching BPR-1200's own `qt_composed_home_refusal` course. If the owner rejects BPR-1200's rule, this expectation goes back with it |
| `qt_workbench_window_scale_*` | the 200-pixel reachability helper measures the Agent tools column | it measures the Notes page showing, and the course checks both pages |
| `qt_workbench_window_scale_*` | the tray course inspects agent surfaces after choosing Notes | opens Agent tools first |
| `qt_workbench_window_scale_*` | the purpose label is no wider than the Agent tools column | no wider than the Notes pages' tab widget it sits above |

No product file changed. All twelve `qt_{composed_github,document_tabs,workbench_focus_order,workbench_window}_scale_*`
rows pass (the other six with [[BPR-1241]]), and so do `qt_composed_home_refusal_scale_*`
and the tour editor rows (15 of 15).
