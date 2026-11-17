# Review-report follow-ups (BPR-2231 to BPR-2238 less BPR-2236): checkpoint 1 (2026-11-16)

One worker, in the coordinator's order 2234, 2238, 2233, 2237, 2232, 2231,
2235, then one batched look for the visible cards.

## Done (lane commits; main mirrors each card move)

- BPR-2234 `74429be59`: Not reviewed as the thing, then why (prompt and
  report format). Real Sonnet run passed AC-b (scratchpad only).
- BPR-2238 `c3f52b4ca`: code box width read from the finding cell.
- BPR-2233 `ee65ed5a1`: `core::takes_selected_file`; course presses once.
- BPR-2237 `ffb4312ae`: `core::prompt_sent_unedited`; `Edited` on a turn.

## Plan for the rest

- BPR-2232 and BPR-2231 together: a report finding maps to its Agent tab
  card by place (the `Suggested` match in `core::report_findings`), the
  finding's state is the card's (stored by `AgentFindingCards`), the
  report's keep goes through the card's Session/Ordinal; Draft all from the
  report runs the host's Draft all path in the report's findings order.
- BPR-2235: `tour_ask` carries the prompt's name for an unedited send; the
  host remembers it in `PromptTurnNames`; the tour chat bubble shows glyph,
  name and Show prompt.
- Then freeze (Fixture agent, a fixture with a review report), a fresh short
  session with one Accept/Reject a card, SESSION_LINE in each.

## Rules in force

Colour worker (BPR-2236) edits report_page / agent_report_view /
agent_tab_view colours: stage only this work's hunks; never the whole-tree
`coding-format` target (it reformats others' files): format named files with
`_coding-standard/scripts/format.py`.
