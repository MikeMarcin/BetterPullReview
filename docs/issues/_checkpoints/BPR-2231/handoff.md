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

# Checkpoint 2 (2026-11-16): five done, look filed, BPR-2232 and BPR-2231 not started

- Done: BPR-2234, BPR-2238, BPR-2233, BPR-2237, BPR-2235 (`aac47473b`).
- Look `review-followups-v001`: session `6e525e076ba6400a9dbc677a2c9bbd88`,
  state `~/.attended-review-state/review-followups-v001-2026-11-16/`, frozen
  root `~/review-followups-v001.pn_cb0u7` from `0feba206bd70`. Three
  questions: `suggestion-in-card` (2238), `edited-prompt-shows-text` (2237),
  `tour-chat-prompt-turn` (2235, renders: the fixture has no tour). On the
  verdicts: stop the host by its pid, remove the root with
  `scripts/freeze_attended_build.py remove`.
- BPR-2232 and BPR-2231 are still in Backlog, untouched; the worker's context
  ran out. Design read so far: `core::report_findings` already matches a
  report finding to the turn's card by place (`Suggested`); give each finding
  its card's index, take the finding's state from the card's
  (`agent_finding_card_disposition`, kept by `AgentFindingCards`), send a
  report keep through the card's Session/Ordinal (`window_quick_ask.cpp`
  `set_draft_dispatch`), and give card-less findings a record keyed by a
  digest of the report's text and the finding's outline. Draft all from the
  report then plans with `core::plan_draft_all` over those states and runs
  `draft_all_agent_cards` (which ends by showing the Agent tab: the report's
  run should not). They need their own look.

# Checkpoint 3 (2026-11-16): BPR-2232 and BPR-2231 built, look being frozen

- `a7b9c2b20` (main `89434ccdf`): both cards In Progress.
- `fe23e6040`: the change (core `agent_report_keep`, `report_finding::Card`;
  host `settle_report_finding`, report Draft all through
  `draft_all_agent_cards`; Qt report view records, Draft all row).
- Passing on `fe23e6040` (dev, macOS): `core_agent_report`,
  `core_agent_draft_all`, `core_agent_finding_card_memory`,
  `qt_agent_report_view`, `application_review_report_public_path` (report to
  card and card to report, Draft all, restart), `application_ask_report_public_path`,
  `application_ask_menu_public_path`, `application_agent_keep_action_public_path`,
  `qt_agent_finding_cards_retained`, `qt_agent_finding_cards_restart`,
  `architecture_boundaries`, `architecture_boundary_contract`, docs gate.
- Next: freeze `report-keeps-v001` (nested-folders-v1, renderer.cpp, Fixture
  agent), renders from the frozen tree's course, fresh session with one
  Accept/Reject per card, serve, record the session on the cards.

# Checkpoint 4 (2026-11-16): look report-keeps-v001 filed

- `9ae980844`: code boxes take the column's width (found rendering the look).
- Look: session `0be6df2e1b895302d0be6058915b8289`, state
  `~/.attended-review-state/report-keeps-v001-2026-11-16/`, host pid in
  `serve.pid`, root `~/report-keeps-v001.0rwtd9d6` from `9ae9808447d8`.
- Next: read the verdicts (`status --after 6`), close both cards on Accept
  (mirror to main), stop the host by pid, remove the root.
- Seen, not filed: a restored report turn's byline reads "success" in place
  of how it ended.
