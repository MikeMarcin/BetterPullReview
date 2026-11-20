---
schema: "implement-rfc.card.v1"
id: "BPR-2286"
title: "Ask follow-up on a finding and Ask about this on a draft or thread, with Update suggested comment, Update draft and Draft reply"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2286--BPR-2286--0a85aaf7eeb3.json","origin":"owner, 2026-11-19","related_cards":["BPR-2268","BPR-2219","BPR-2231","BPR-2051","BPR-2281"],"related_rfcs":["RFC-BPR-0128","RFC-BPR-0127","RFC-BPR-0101"]}
---

# BPR-2286: Ask follow-up and Ask about this

## Owner (2026-11-19)

"I want to be able to easily ask a follow-up and perhaps update a suggested
comment from a report or agent tab response. There are many issues showing
up from a code review I either don't fully understand or want the agent to
rewrite. I also want to ask about a comment from a comment or draft
composer, and either be able to optionally update the draft or draft a reply
to a thread after one or more rounds of conversation with the agent."

## Change

- **Ask follow-up** on a finding: on an Agent tab finding card (beside Draft
  and Dismiss) and on a report's finding (a chip beside Draft comment). It
  opens a follow-up window about that finding: its place, the finding and its
  suggested comment, quoted. The reviewer asks; each answer is a round, and
  every round carries the rounds before it. At any point **Update suggested
  comment** asks the agent for the comment as the conversation settled it,
  shown beside the comment it replaces with **Use this** and **Discard**.
  Nothing changes until Use. Then the card shows the new comment, the report
  shows "Suggested comment, updated:" under the finding, and Draft comment
  (the card's, the report's, Draft all) drafts the new text. A report finding
  at a card's place is that card, so both ask in the same conversation.
- **Ask about this** on the draft composer (a new comment or a reply, beside
  Enhance) and on a thread's Ask menu (**Ask about this…**). With text in the
  field it is about the draft and offers **Update draft**: Use replaces the
  field's text as one undoable edit, only if the field still holds the text
  the proposal was made from. With an empty reply field it is about the
  thread and offers **Draft reply**: **Put in reply** puts the reply in the
  thread's reply composer, after any words already there.
- Nothing is posted. Every apply needs a press on Use after the text is
  shown; the reviewer publishes as before.

## Acceptance

- AC1. Core: a follow-up's rounds, each run's identity and message (the
  item, the earlier rounds bounded with the newest kept, the question or the
  apply instruction), stale and failed answers, proposals read and refused
  (empty, too long, unchanged), and Use refused for a stale proposal or a
  changed text; equal state and question give an equal message.
- AC2. Host: rounds run on the agent lane and settle on the decision lane;
  follow-ups are kept by item; Stop, failure and shutdown; the host applies
  nothing.
- AC3. Qt: the follow-up window's intents, the running Stop in the ordinary
  roles and Ask/apply in the Agent accent, the proposal beside what it
  replaces, refusals in words, the per-item apply action; a report finding's
  Ask follow-up chip and updated comment; a thread's Ask about this row.
- AC4. Public path with the fixture agent through the window: the draft
  composer, a thread and a review's finding card and report, as above.
- AC5. architecture_boundaries, coding-format-check, tidy of the changed files
  and the documentation gate pass.

## Decisions

- **Where the conversation is.** A follow-up opens in a window of its own,
  one conversation per item, rather than in the Agent tab. The Agent tab is
  the pull request's one broad conversation, and a scoped one ending in an
  apply would mix subjects there; a separate window keeps the report or the
  composer in view beside it. Opening the same item again brings back its
  conversation for as long as the window is open.
- **How rounds run.** As Draft Description does ([[BPR-2281]]): each round is
  one fresh, tool-free run of the window's agent on its own lane, a new run
  path `follow-up` (fresh, no Ask more). The message carries the item and the
  earlier rounds, so a conversation does not depend on an agent's own
  session. Its instructions are a fixed system prompt
  (`core::follow_up_instructions`) and the profile's Writing style.
- **Proposals, not edits.** An apply action is a run that asks for the text
  alone; its answer is read like Draft Description's (trimmed, one plain
  fence unwrapped; a `suggestion` fence is kept as the comment's own) and
  shown beside the current text, as Enhance's Use suggestion is. An Update
  over a text changed since it was asked for is refused, so nothing is
  silently replaced.
- **What an updated suggested comment is.** The window's own record, by the
  finding's key (its card's delivery and place, or the report's turn and
  row): the card and the report read it, and Draft comment drafts it. It is
  not written into the stored answer, so it is not kept past the window.
- **Draft reply versus Update draft.** Decided by the field: text in it makes
  the draft the subject (Update draft); an empty reply field makes the thread
  the subject (Draft reply). A thread whose viewer may not reply offers no
  apply action.
- **Ordinary roles for the openers.** Ask follow-up and Ask about this open a
  window and send nothing, so they wear the ordinary roles; Ask and the apply
  action start runs and wear the Agent accent (RFC-BPR-0101's rule).
- The frontend-design skill was loaded for the window, the chips and the
  buttons.

## Verification

macOS, dev preset, lane at `589072fb5` plus this change. `coding-format`
on every changed C++ file; `tidy.py` on the changed translation units reports
nothing in this change's lines (its findings are in code it did not touch).

27 focused rows, 25 pass: `core_agent_follow_up` (AC1),
`application_follow_up_host` (AC2), `qt_follow_up_window` (AC3),
`application_follow_up_public_path` (AC4: the draft composer's Ask about
this, a refused Use over a changed draft, Update draft as one undoable edit;
a thread's Ask about this…, Draft reply into its reply composer; a review's
tonemap card, two rounds (the second carrying the first), Update suggested
comment unseen until Use, then on the card, beside the report's finding, Ask
follow-up from the report reaching the same two rounds, and Draft comment
drafting the new text; nothing published), `application_agent_launch_policy`
(the `follow-up` path is fresh), `qt_agent_report_view`,
`qt_agent_tone_colours`, `qt_conversation_cards_view`, `qt_ask_menu`,
`qt_enhance_control`, `qt_button_faces`, `qt_drawer_surfaces_fit_scale_100`
and `_200`, `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`,
`application_review_report_public_path`,
`application_agent_keep_action_public_path`,
`application_ask_menu_public_path`, `application_suggest_change_public_path`,
`application_enhance_comment_note_public_path`,
`application_draft_description_public_path`,
`application_fixture_agent_every_ask`,
`application_fixture_agent_guard_public_path`, `fixture_agent_answers`,
`core_suggest_change`.

Not this change: `architecture_boundaries` fails only on another lane
worker's uncommitted `gh_cli_git_smart_transport.cpp`, and
`test_network_isolation_manifest` only on rows other cards registered
(`sqlite_prompt_pack_storage`, `application_agent_chat_held_turn_public_path`,
`application_window_file_drop_public_path`); this change's own row is
entered. Both are rerun on the landed commit.

No real agent and no live GitHub: every run is the fixture agent's and
nothing is written to GitHub.

## Not done

- The follow-up's system prompt is not editable in Settings › Prompts.
- An updated suggested comment is not persisted past the window.
- The Review mode's own draft editor (`review_draft_editor`) has no Ask about
  this; the drawer's composer, where drafts are written, has it.
- No attended look was prepared.
