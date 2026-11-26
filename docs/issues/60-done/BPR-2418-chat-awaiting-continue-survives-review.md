---
schema: "implement-rfc.card.v1"
id: "BPR-2418"
title: "A chat still waiting for its Continue answer survives Review reading the comparison again, and the Agent tab's card answers it from the tab"
status: "Done"
rfcs: ["RFC-BPR-0012", "RFC-BPR-0096", "RFC-BPR-0130"]
depends_on: ["BPR-2412", "BPR-2413", "BPR-2417"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"70d755954","origin":"found by the BPR-2417 implementer, 2026-11-26","related_cards":["BPR-2379","BPR-2412","BPR-2413","BPR-2417"],"related_rfcs":["RFC-BPR-0012","RFC-BPR-0096","RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
acceptance: []
---

# BPR-2418: The tab's Continue, and a chat that survives Review

## Report

The BPR-2417 implementer reported two things in its "Not in this card":

1. In the head-moved state, pressing the Agent tab's own "Continue at
   <sha>" (`agent_tab_view::choose_on_banner` to `choose_continuation`) did
   not reach the chat host, while the side chat panel's Continue did.
2. Entering Review after the head moved, then Continue: in one exploratory
   run the continued turn's re-alignment came back `host_failure`.

## Findings

**1. The tab's card was wired.** Its Continue and New session leave the
tab as `agent_tab_intents::Continue`, which the window maps to the side
chat's continuation intent and the chat host's `choose`. The reconcile
choices and the pinned card's choices reach the host through their own
dispatches. Pressed while the card shows, Continue and New session reach
the host: the `stale` and `continued` courses now press them and pass.

What the implementer saw comes from the anchored course's own setup. Its
Review entry loads the clone link the agent setup waits on. The setup
cannot see the fixture agent the course composes ("No agent found on this
computer"), so it takes the tab's place: the tab is `setting_up`, and the
card and its buttons are hidden. A press on the hidden button then falls to
`choose_pinned`, which has nothing to answer. A reviewer cannot make that
press. The anchored course therefore keeps answering with the side chat's
action, and says why.

**2. A real bug in that order, but not `host_failure`.** Two orders
reproduce it:

- head moves, Continue, comparison moves, Review;
- head moves, Review.

In both, the chat waits for an answer when Review reads the comparison
again. When the comparison moves, `observe` clears the Continue choice and
asks again. While the comparison was absent, `refresh_agent_authority`
kept the chat only while a Continue choice stood (BPR-2417). Otherwise it
cleared the chat, and a fresh session at the head under review took the
kept session's place without asking. The reviewer's conversation and the
card's question were both lost.

`host_failure` did not reproduce. With the fix, both orders ask again.
After the answer, the continued turn is composed at the old revision:
pinned, recorded and kept bindings are equal, `realignment:none`, and the
identities match byte for byte. In these orders the turn is not
retargeted, because Review's comparison is the kept pair's epochs.

## Fix

- **Application.** `agent_chat_session_host::awaits_continuation()` is true
  when the core's `offer_agent_continuations` requires an answer and none
  was given.
- **Composition.** `refresh_agent_authority` keeps the chat across a
  same-review re-read when it waits for that answer, as it already did
  for a standing Continue choice. The chat asks again once the
  comparison lands. A different review still clears it.

Not changed: New session, the follow-the-head move, a history reload's
verdict (`core::decide_conversation_reload`), and the tab's intents.

## Acceptance

- AC1. A Qt test shows that the Agent tab's card buttons, in the head-moved
  state, leave the tab as the chat's `choose_chat_continuation_intent`:
  Continue at the revision read, then New session, for the session shown.
- AC2. `agent_chat_session_host::awaits_continuation` is true only for a
  session that read another revision with no answer given. It is cleared
  by Continue and by New session, and set again when the comparison moves
  after an answer.
- AC3. The `stale` course chooses New session on the tab's card. The
  `continued` course chooses Continue on the tab's card, then enters Review
  after the comparison moved, and requires the kept session (same id and
  binding) to survive and ask again. It fails before the fix.
- AC4. The agent chat and invocation contracts pass.
- AC5. Format and tidy on touched units.

## Evidence

Revision `70d755954` (on main `8e2fbaa90`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `qt_workbench_agent_send_refusal` passed. Its
  `test_a_pinned_conversation_card_offers_what_it_names` presents a stale
  session and clicks `banner_first_button` and `banner_second_button`. It
  requires one `continue_against_old_revision` intent with the read
  binding, then one `start_new_session` intent. The wiring was already
  right, so this test also passes on main.
- AC2: `application_agent_chat_replies` passed (the new
  awaits-continuation block).
- AC3: `application_agent_chat_stale_contract`,
  `application_agent_chat_continued_contract` and
  `application_agent_chat_anchored_continuation_contract` passed.
  Negative check: with the composition's new clause disabled, the
  continued course fails with `late-review-kept kept=0 asked=0`, which is
  the reported loss.
- AC4: 23 of 23 passed:
  - `application_agent_invocation_host`;
  - `application_agent_chat_replies`;
  - `qt_workbench_agent_send_refusal`;
  - `application_agent_chat_composition_contract`, plus the
    `application_agent_chat_` contracts `stale`, `continued`,
    `anchored_continuation`, `construction_cut`, `conversation`,
    `custom_agent`, `custom_agent_refused`, `subject`, `setup_list` and
    `head_move_send`;
  - `application_agent_invocation_composition_contract`, plus the
    `application_agent_invocation_` contracts `construction_cut`,
    `launcher_construction_cut`, `gesture_cut`, `cancellation`,
    `staleness`, `cleanup_retry`, `workspace_diagnostic` and
    `native_pinned`.
- AC5: `_coding-standard/scripts/format.py --dry-run` (clang-format 22)
  reports no violation in the six touched files. `tidy.py` passed over
  `agent_chat_session_host.cpp`, `window_agent_chat.cpp`,
  `agent_chat_reply_tests.cpp`, the composed course and
  `workbench_window_tests.cpp`. None of its findings is on a changed line.

## Not in this card

- In the anchored course, the agent setup stands in the Agent tab once
  Review opens, because the setup cannot see the fixture agent composed
  through `CodexAgentExecutable`. Pressing the tab's card there would need
  the course's agent to be a set-up agent (an entry in the setup store, a
  linked repository and a passed check). That is a fixture change, not a
  product one.
- `choose_on_banner` routes by the tab's `State`. A press on a hidden card
  (while the setup shows) falls to `choose_pinned`. A reviewer cannot make
  that press, so it is left as is.
