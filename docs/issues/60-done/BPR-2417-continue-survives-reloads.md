---
schema: "implement-rfc.card.v1"
id: "BPR-2417"
title: "A reviewer's Continue at <old> survives the reloads they did not ask for, so the next turn runs at the revision they chose"
status: "Done"
rfcs: ["RFC-BPR-0012", "RFC-BPR-0096"]
depends_on: ["BPR-2379", "BPR-2412", "BPR-2413"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"95a14d68b","origin":"found by the RFC-BPR-0130 stage 6 worker (BPR-2413), 2026-11-25","related_cards":["BPR-2379","BPR-2412","BPR-2413","BPR-2414"],"related_rfcs":["RFC-BPR-0012","RFC-BPR-0096","RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
acceptance: []
---

# BPR-2417: Continue at <old> survives a reload

## Report

The RFC-BPR-0130 stage 6 worker found this. The reviewer chooses "Continue
at <old sha>" on the pinned-revision card or with the side chat's Continue
against the old revision. Opening a file in Review then reloads the Agent
tab's session history. When that reload finished,
`present_agent_native_history` called
`agent_chat_session_host::reset_conversation()`. That call moved the kept
session's binding to the head under review and dropped the choice, so the
next turn ran against the new head without a word. The reviewer had chosen
to keep talking about the old revision, and it changed under them.

Two more paths did the same:

- The native restore that follows a reload (`NativeSession`) could
  re-admit the conversation at the new head (the BPR-2379 move), and
  `NativePinned` could re-pin it.
- Entering Review reads the comparison again. While the comparison was
  absent, `refresh_agent_authority` cleared the chat host outright. A fresh
  session then started at the new head, with nothing left to answer.

BPR-2412 and BPR-2413 left
`application_agent_chat_anchored_continuation_contract` sending its
continued question through the side chat panel because of this.

## Fix

The core decides what a reload keeps. The host performs.

1. **Core**: `core::decide_conversation_reload(conversation_reload_facts)`
   returns `keep_continuation` when all three hold:
   - a `continue_against_old_revision` choice stands (the revision under
     review has not moved since);
   - the reload selected the saved conversation the choice was made in;
   - the reload is not the reviewer's own pick (History or New session).

   Every other case returns `start_at_observed`, which is the behaviour
   before this card.
2. **Application**: `agent_chat_continuation_choice` carries the saved
   conversation shown when the reviewer chose it (`Conversation`).
   `agent_chat_session_host` stores that conversation with the choice. Two
   new calls ask the core: `reload_conversation(selected, chosenAnew)`,
   which resets only on `start_at_observed`, and
   `keeps_continuation(selected)`.
3. **Composition**:
   - Both Continue paths, the pinned card and the side chat's choice, pass
     `AgentNativeHistory.Selected`.
   - A History pick or New session marks the next history load as chosen
     anew.
   - `present_agent_native_history` calls `reload_conversation` in place of
     `reset_conversation`.
   - `NativeSession` and `NativePinned` leave the chat alone while a
     Continue choice stands for the selected conversation.
   - When the comparison is read again for the same pull request, the chat
     is not cleared while a Continue choice stands. The chat is observed
     again when the comparison lands, and a different comparison asks
     again.
4. **Test seam**: `github_application_fixture_access::replay_agent_history_load`
   presents the window's last saved-history view again, finished, through
   `present_agent_native_history`. The fixture agent keeps no native
   history, so before this no course reached that reload.

Not changed:

- New session, from History, from the card or from the side chat, starts
  at the revision under review as before.
- A conversation whose workspace can follow the head still moves on a
  plain send when nothing was continued (BPR-2379's move).
- When the head moves again, the choice is still cleared and the reviewer
  is asked again.

## Acceptance

- AC1. `core::decide_conversation_reload` keeps a standing Continue choice
  only for a reload of the continued conversation that the reviewer did not
  ask for. Equal facts give equal verdicts.
- AC2. After Continue, a reload of the same conversation keeps the chat's
  session at the old revision and asks nothing, and the next turn is
  dispatched at that revision. A History pick or New session, another
  conversation, or a second head move starts at the revision under review.
- AC3. `application_agent_chat_anchored_continuation_contract` sends its
  continued question through the Agent tab's own field and Send, about the
  lines selected in Review. It sends after a history reload, without
  answering the card again, and still requires the anchored finding, the
  re-aligned freeze and the old revision's binding.
- AC4. The agent invocation and chat contracts and the native pinned
  contract still pass.
- AC5. Format and tidy on touched units.

## Evidence

Revision `95a14d68b` (on main `c62c2caae`, which includes BPR-2414), macOS
arm64 Debug, `ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `core_agent_session` (`test_a_reload_keeps_a_continue_choice`)
  passed.
- AC2: `application_agent_chat_replies` passed. Its reload-after-Continue
  block checks that a reload of the continued conversation keeps the
  choice and that the next turn is dispatched at the old binding. It also
  checks that a History pick or New session, another conversation and a
  second head move start at the observed revision.
- AC3: `application_agent_chat_anchored_continuation_contract` passed. The
  course enters Review before the head moves, continues at the old
  revision, opens the file, replays the history load and selects lines
  (the tab's lines chip shows). Before pressing the tab's Send once, it
  requires that the chat still holds the kept session, asks nothing and
  offers Send.

  Negative check: with the four `window_*` composition files at main and
  the test changes kept, the course fails with
  `anchored-continue-kept required=0 kept=0 send=1`. The session moved to
  the head on screen and nothing was asked: the reported bug.
- AC4: 23 of 23 passed:
  - `application_agent_chat_composition_contract`, plus the
    `application_agent_chat_` contracts `stale`, `continued`,
    `construction_cut`, `conversation`, `custom_agent`,
    `custom_agent_refused`, `subject`, `setup_list` and `head_move_send`;
  - `application_agent_invocation_host`;
  - `application_agent_invocation_composition_contract`, plus the
    `application_agent_invocation_` contracts `construction_cut`,
    `launcher_construction_cut`, `gesture_cut`, `cancellation`,
    `staleness`, `cleanup_retry`, `workspace_diagnostic` and
    `native_pinned`.
- AC5: `_coding-standard/scripts/format.py` (clang-format 22) reports no
  violation in any touched file. `tidy.py` ran over `agent_session.cpp`,
  `agent_chat_session_host.cpp`, `window_agent_chat.cpp`,
  `window_agent_setup.cpp`, `window_construction.cpp`,
  `fixture_effect_holds_and_agent_evidence.cpp` and the three test files.
  It reported nothing on a changed line. The composed course's other
  findings are on lines this card does not touch.

## Not in this card

- In this course, pressing the Agent tab's own "Continue at <sha>" in the
  head-moved state (`agent_tab_view::choose_on_banner` to
  `choose_continuation`) did not reach the chat host: the chat's
  continuation count stayed the same. The side chat panel's Continue action
  did reach it. The course answers the head move with the panel's action,
  as it did before. This needs its own look; stage 7 owns the side chat
  panel's controls.
- Entering Review after the head moved reads a different comparison, which
  asks again by design. In one exploratory run, a continued turn's
  re-alignment then returned `host_failure`. The course enters Review
  before the head moves, so it does not cover that order.
