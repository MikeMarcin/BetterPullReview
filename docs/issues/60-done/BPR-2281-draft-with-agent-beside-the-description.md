---
schema: "implement-rfc.card.v1"
id: "BPR-2281"
title: "Draft Description sits beside the description in Author, from a system prompt, not in the Ask menu"
status: "Done"
rfcs: []
depends_on: ["BPR-2280"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"change","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2281--BPR-2281--ff2554fb5b87.json","decided_by":"owner, 2026-11-19","related_cards":["BPR-2280","BPR-2274"]}
---

# BPR-2281: Draft Description sits beside the description

## Owner (2026-11-19, on BPR-2280)

"I wouldn't put draft description in the Ask menu, I would put it near the
description field in Author." Then: it "works like Draft Tour, it's one of
those system editable prompts", and "only if that makes sense, it's not
related to tour anymore".

## Change

- No Ask-menu or chip prompt drafts the description: [[BPR-2280]]'s shipped
  `agent-tab.draft-description` prompt and its `prompt_ask::description` kind
  are gone, with the Agent tab's Use as description.
- Author's Workflow tab offers **Draft Description** beside the description's
  Edit/Write, styled as Draft Tour is (bordered, Agent accent; Stop drafting
  while it runs). It runs the **Description writing** system prompt (editable
  in Settings › Prompts) with the window's agent and puts the answer in the
  description editor as one undoable edit. The line beside the field says it
  is drafting, that it stopped, or why it drafted nothing. Nothing is sent
  until Save.

## Acceptance

- AC1. No shipped prompt drafts the description; nothing is offered in the
  Ask menu or as a chip; a stored or queued `description` kind reads as a
  question (core tests).
- AC2. Public path with the fixture agent: Draft Description fills the
  editor with the description, says it is drafting while it runs, sends
  nothing until Save; Save writes once; a stale save is refused.
- AC3. Qt test: the button, its running, stop and failure states, and the
  fill as one undoable edit.
- AC4. architecture_boundaries, coding-format-check and the documentation
  gate pass.

## Decisions

- Not tour code. Draft Tour runs through the tour session (tour runs, tour
  threads, the tour editor's running state), which the owner said this is not
  part of. The generic piece is the agent runner the window already builds for
  one-shot, tool-free writing runs (a local draft's description and its commit
  message use it). `application::description_draft_host` runs it on its own
  lane, as `local_changes_host` runs the commit message: stoppable, its answer
  back on the decision lane, a stale answer dropped, part of the window's
  shutdown and quiescence.
- The system prompt is the existing **Description writing** hook
  (`local-description`), generalised from local drafts to any pull request and
  now asking for `## Summary`, `## What changed`, `## How to test` with no
  title, preamble, findings or comment label: `local-description-v2`. No new
  hook, so no new settings column.
- `core::compose_description_draft` is the run's message: a marker line
  (`bpr.description-draft.v1`), the title, the current description (bounded,
  to rewrite) and each file's patch until a 192 KiB bound, the rest named as
  left out. `core::read_description_draft` trims, unwraps one fence around the
  whole answer, and refuses an empty or over-long answer.
- `prompt_ask::description` is removed rather than kept: nothing needs it
  once the Ask placement is gone. It existed on main for a few hours; a
  stored prompt, prompt pack or queued prompt naming it reads as a question.
- The label is **Draft Description**, the same shape as **Draft Tour**.

## Verification

macOS, dev preset, lane at `37b744c54` plus this change. Full dev build;
`coding-format-check` passed. The documentation gate's only finding is
another card's (BPR-2283 depends on BPR-2282, both In Progress in the lane),
not this change.

38 of 39 focused rows pass: `core_description_edit` (the draft's material:
marker, title, description, patches, the bound; the answer reader),
`core_prompt_macro`, `core_prompt_surfaces`, `core_quick_ask`,
`qt_prompt_macro_chips` (AC1: no shipped prompt drafts the description; not
in the Ask menu or chips), `core_agent_queue` (a queued retired description
kind reads as a question), `core_prompt_pack`, `sqlite_prompt_pack_storage`,
`sqlite_agent_configuration_storage` (`local-description-v2` exports),
`application_description_draft_host` (one run at a time; the answer on the
decision lane; failure, empty answer, stop, refusal, shutdown),
`application_writing_delivery` (the system prompt's three parts),
`qt_author_description_editor` (AC3), `application_draft_description_public_path`
(AC2: no Ask-menu row; Author's Draft Description says it is drafting beside
the field, then the editor holds the fixture agent's three-part description
as one undoable edit, nothing sent; Save writes once and the next read shows
it; a description changed on GitHub is refused, nothing written, Reload),
`fixture_agent_answers`, `application_fixture_agent_every_ask`,
`architecture_boundaries`, `qt_settings_window`, `qt_prompts_page`,
`qt_ask_menu`, `qt_workbench_window_scale_100`, `qt_pull_request_overview_page`,
`application_description_edit_contract`, the Ask-menu, keep-action,
commit-asks, prompt-macro, prompt-facts, chat-composition and agent-queue
public paths, the local-draft author, workflow and tour courses and
`tour_cli_composition`.

Not this change: `test_network_isolation_manifest` still names
`sqlite_prompt_pack_storage` and `application_agent_chat_held_turn_public_path`
([[BPR-2274]]).

No live GitHub write and no real agent: the course's runs are the fixture
agent's and its writes the fixture's.
