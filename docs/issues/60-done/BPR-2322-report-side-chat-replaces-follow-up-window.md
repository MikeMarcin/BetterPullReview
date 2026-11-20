---
schema: "implement-rfc.card.v1"
id: "BPR-2322"
title: "The report tab's side chat replaces the follow-up window"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux-refactor","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2322--BPR-2322--b934dff5c89b.json","origin":"owner, 2026-11-20","related_cards":["BPR-2286","BPR-1925","BPR-1995","BPR-1914"],"related_rfcs":["RFC-BPR-0128","RFC-BPR-0100","RFC-BPR-0101"]}
---

# BPR-2322: The report tab's side chat replaces the follow-up window

## Owner (2026-11-20)

"I just finally saw the report ask follow-up. It looks like it implements
another completely new agent interface. I think we should instead make it use
something akin to the side chat from the tour onto the report tab."

## Change

[[BPR-2286]] opened a separate follow-up window with a look of its own for Ask
follow-up (a report finding, an Agent tab finding card) and Ask about this (the
draft composer, a thread's Ask menu). That window goes. In its place a side
chat column, built from the Tour tab's side chat parts ([[BPR-1925]],
[[BPR-1995]]) and wearing its look, opens beside the report (and beside the
code for a draft or a thread): the seam in the agent accent, the item it is
about at the top with Copy chat and Close, the reviewer's words in bubbles on
the right, the agent's replies as Markdown on the left with the Agent tab's
ending line, and the composer with the item as a chip.

The conversation model and its host are kept: `core::agent_follow_up` and
`application::follow_up_host` (one conversation per item, fresh tool-free
rounds, proposals that apply only on Use).

## Acceptance

- AC1. The follow-up window (`qt::follow_up_window`), its tests and its
  documentation are gone; no window opens for Ask follow-up or Ask about this.
- AC2. The side chat column shares the tour side chat's parts (the author's
  bubble, the proposal frame, Return-to-send, the composer field, the
  passage chip, the seam), moved into one shared unit both use, not copied.
- AC3. Ask follow-up on a report finding opens the side chat beside the
  report, about that finding (its place and suggested comment as the chip);
  asking shows the question and the agent's answer as turns; Update
  suggested comment shows the proposal beside the comment it replaces with
  Use and Discard; Use updates the finding's suggested comment and the
  report says so. An Agent tab card's Ask follow-up opens the same chat.
- AC4. Ask about this on the draft composer opens the side chat about the
  draft and offers Update draft (one undoable edit, refused over a changed
  draft); on a thread with an empty reply field it offers Draft reply, which
  puts the reply in the thread's reply composer. Nothing is posted.
- AC5. Closing the column keeps every item's chat; opening the same item
  again shows its turns.
- AC7. Owner, 2026-11-20 ("Make sure that we can see things like these
  follow-up questions being asked in the logs and being applied", and
  "make sure that the actual content of messages and agent responses don't
  show up in any anonymized exports"): each question, proposal and Use is
  a Standard `agent.side_chat` line with the item's kind, the round, the
  apply action, the outcome or refusal, the duration and the texts'
  lengths, and no words; an anonymized export of the log holds none of
  the words asked, answered, proposed or quoted.
- AC6. Qt tests for the side chat (open, ask about an item, revise a
  suggested comment and a draft, draft a reply, close and reopen), the public
  path course moved onto the side chat, `architecture_boundaries`,
  `coding-format-check` and the documentation gate pass.

## Decisions

- The frontend-design skill was loaded for the column.
- What the column is built from: the tour's side chat widget itself is
  bound to the tour session (stops, revisions, queues, native history), so
  it is not reused whole. Its parts are: `side_chat_parts.{hpp,cpp}` holds
  the seam, the bubble, the proposal frame, Return to send and bubble
  fitting, moved out of `tour_side_chat.cpp`, which now uses them; the
  public `composer_field`, `passage_chip`, `side_chat_icon`,
  `agent_run_status_line`, `markdown_body_view` and `make_tour_action` are
  used as they are.
- Where the column is: on the comparison page, beside the report or beside
  the code. A draft or a thread has no report, so its chat stands beside
  the code it is on. Asked from the Agent tab while neither shows, the code
  at the finding's place is brought up.
- The conversation stays `core::agent_follow_up` and
  `application::follow_up_host`; the tour's conversation host is the tour
  session's and is not generalized.
- Logging is content-free at every detail for now. Content at Verbose, the
  finding's key and agent on each line, Discard, Undo and Redo lines are
  [[BPR-2323]]'s.

## Verification

macOS, dev preset, `build/dev` at `ddebda773` (this change on main
`1b11c3a8d`). `coding-format` on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `qt_follow_up_window` and `follow_up_window.{hpp,cpp}` deleted; `git grep follow_up_window` finds only cards | pass |
| AC2 | `qt_item_side_chat` (both chats draw the same seam, both compose in `composer_field`); `qt_tour_side_chat` | pass |
| AC3, AC5 | `qt_item_side_chat` (open, ask, live line then answer, Update suggested comment, Use and Discard, refusals, Copy chat, close and reopen); `application_follow_up_public_path` (Agent tab card, then the report's Ask follow-up beside the report: the same chat, two rounds; Draft comment drafts the update) | pass |
| AC4 | `application_follow_up_public_path` (draft composer: Update draft, refused over a changed draft, one undoable edit; thread: Draft reply into its reply composer; nothing published) | pass |
| AC6 | `qt_stick_to_bottom`, `qt_workbench_window_scale_100`, `architecture_boundaries`, `coding_format_check`, `verify_documentation.py` | pass |
| AC7 | `core_operation_diagnosis` (side chat line grammar), `application_follow_up_host` (asked, proposed, applied, refused, failed and stopped lines with lengths; no words), `application_release_maintenance` (a side chat line exports; one with words loses its detail), `diagnostic_controls`, `application_operation_diagnostics`, `application_host_operation_diagnostics`; `application_follow_up_public_path` with Verbose on: the log has asked, proposed and applied lines for all three items, and neither the log nor its anonymized export holds any of ten marker phrases | pass |

Not this change:
- `test_network_isolation_manifest` fails only on rows other cards
  registered (`sqlite_prompt_pack_storage`, `git_haves_trimmed_pack_historical`
  and its `-wall`, `application_agent_chat_held_turn_public_path`,
  `application_agent_suggestion_range_public_path`,
  `application_window_file_drop_public_path`). No row of this change is
  among them.
- `application_review_round_trip_composition_contract`: the production app
  digest is resealed and passes. The course then stops at
  `SEMANTIC_COMMENT_PROBE_FAILED phase=4`, which [[BPR-2269]] recorded as
  predating it.
- `application_helper_tour_round_diagnostics` does not compile on main
  (`application::cycle_store_failure` no longer exists).

No real agent and no live GitHub: every run is the fixture agent's, and
nothing is written to GitHub. No attended look was prepared.
