---
schema: "implement-rfc.card.v1"
id: "BPR-1847"
title: "An agent turn that ended without an answer is an error, not a reply to save, and suggested prompts wait for the setup"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"owner, 2026-11-03, with a photograph of the Agent tab: \"Agent wizard errors shouldn't get the save as note and draft comment treatment. Also doesn't make sense to show suggested prompts before the wizard completes.\"","reservation":"docs/issues/_reservations/BPR-1847--BPR-1847--bd6c0a13aa55.json","related_cards":["BPR-1753","BPR-1755","BPR-1773","BPR-1817","BPR-1827","BPR-1772"],"frontend_design":"loaded (frontend-design:frontend-design) before the visual change; the error reuses the danger look of the stopped banner and the send refusal rather than a new style","copy_choice":"Copy is kept on an error turn: it copies the error's words and the tail of what the agent wrote to its error stream, which is what a report or a search needs"}
acceptance: [{"criterion": "AC1 The core decides an ended turn's actions from how it ended: an answer offers Copy, Save as note and Draft comment; every non-answer is an error that keeps Copy when it has words, never offers Save as note or Draft comment, and on the latest turn while nothing runs offers one recovery (Agent settings for an unreadable answer, Check again for a start or workspace failure, Ask again otherwise)", "outcome": "pass", "revision": "19a60cbdf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_agent_turn$'", "detail": "agent_turn_tests.cpp: every outcome x latest x working; names and ordinals; legacy ending inference."}}, {"criterion": "AC2 How each turn ended is kept with the stored conversation, so a thread opened again presents its errors as errors; a turn stored before this reads back from the ending this application wrote", "outcome": "pass", "revision": "19a60cbdf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(sqlite_agent_setup_storage|application_agent_conversation_host|qt_composed_github_scale_100)$'", "detail": "Outcomes round-trip through agent_conversation_outcomes; an undeclared ordinal is refused by the schema; with the outcome rows removed, 'Stopped after ...' reads back as ended_without_answer and an answer as answered; Clear session removes both."}}, {"criterion": "AC3 The Agent tab presents an error turn as an error (danger look, accessible description Error), with Copy and its recovery and without Save as note or Draft comment, live and restored; the suggested prompts are hidden while the setup is unfinished (agent or repository to choose, a check failed or running, the setup on screen, its Ready line up) and Finish setup stands in their place", "outcome": "pass", "revision": "19a60cbdf", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_workbench_agent_send_refusal$'", "detail": "test_an_agent_error_is_an_error_and_suggestions_wait_for_setup: protocol_failure run, restored three-turn thread, failed_to_start run, failed workspace check, Finish setup, Ready line, a reopened set-up application. The full default workbench run also passes (123 cases, exit 0)."}}, {"criterion": "AC4 Gates: clang-format 22, clang-tidy on the changed translation units, the documentation gate, the command-correctness registry", "outcome": "pass", "revision": "19a60cbdf", "evidence": {"kind": "command", "command": "cmake --build build/dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors (7 TUs); python3 scripts/verify_documentation.py --root .; ctest -R '^command_correctness_registry_contract$'", "detail": "Formatting check passed (clang-format 22.1.8, 1898 files). clang-tidy clean on agent_turn.cpp, agent_turn_tests.cpp, agent_launch.cpp, agent_setup_store.cpp, agent_tab_view.cpp, agent_setup_storage_tests.cpp; github_application.cpp reports one pre-existing readability-identifier-naming error at line 33565 (go_on_to_reviewers, from cfe4440c5), outside this change. DOCUMENTATION_OK. command_correctness_registry_contract passes with the workbench source digest updated."}}, {"criterion": "AC5 The owner judges the error turn and the pre-setup panel, light and dark: Accept or Reject (one question each)", "outcome": "pass", "revision": "57a2bb387", "evidence": {"kind": "observation", "detail": "Attended review session ebb2b3b1123ec944ecc7f9db7053a348 (state /Users/jaeger/.codex/attended-review/bpr1847-v001-Nn2PgH, frozen BPR-1847-v001 at 57a2bb387), handed back 2026-11-04T00:15Z: the owner accepted all three questions (error turn look; pre-setup panel; prompts hidden while the Ready card is up)."}}]
---

# BPR-1847: Agent errors are errors, and suggested prompts wait for the setup

Owner, 2026-11-03, with a photograph of the Agent tab showing two "Review
this pull request" turns, each answered "declawd answered in a form this app
can't read. Stopped after 0:15." with Copy, Save as note and Draft comment,
and the suggested prompts under them: "Agent wizard errors shouldn't get the
save as note and draft comment treatment. Also doesn't make sense to show
suggested prompts before the wizard completes."

## Causes

- **Save as note and Draft comment on errors.** A live turn already hid
  them when its run failed ([[BPR-1817]]), but a turn read back from the kept
  conversation ([[BPR-1753]], owner decision (b)) carried no record of how it
  ended: `agent_tab_view::present_thread` settled every restored turn as an
  answer. The photograph's turns are restored ones: a live review turn's
  question is in italics, and theirs are not. The stored turn
  (`agent_conversation_turns`: asked, answered, ending) had nowhere to say it
  was an error.
- **Suggested prompts.** They were gated only on the tab's state (not set
  up, setting up, working). The setup's Ready line (its fourth step, drawn as
  the green card above the thread in the photograph) and a setup whose check
  failed or is running never hid them.

## Fix

- Core `bpr/core/agent_turn.hpp`: `agent_turn_outcome` (answered, unreadable,
  exited_with_error, failed_to_start, workspace_unavailable,
  history_unavailable, stopped, timed_out, set_aside, ended_without_answer),
  `decide_agent_turn_actions` and `decide_agent_suggestions`; the outcome's
  stored ordinal and the legacy inference from the ending this application
  wrote (`Stopped after …`).
- Application: `agent_conversation_turn::Outcome`;
  `application::turn_outcome(agent_run_outcome)`.
- SQLite: a side table `agent_conversation_outcomes` beside
  `agent_conversation_turns`, written and cleared in the same transaction and
  bounded with it; a missing row is a turn stored before this card.
- Qt: the reply card takes the danger look and the accessible description
  "Error"; Copy, Save as note, Draft comment and the recovery follow the
  core's verdict on every refresh, so only the latest error, while nothing
  runs, offers a way back. The suggestions follow `decide_agent_suggestions`;
  while the setup is unfinished and not on screen, "Suggested prompts appear
  once the agent is set up." and **Finish setup** (the setup at its first
  unfinished step) stand in their place. Finish setup and the turn's Check
  again open the setup unpinned (`begin(ready)`), so it closes on its Ready
  line once the check passes.
- `docs/ux/workbench-specification.md` describes both.

Copy is kept on an error: it copies the error's words and what the agent
wrote to its error stream, which is what a report needs.

## Why declawd answered in a form this app can't read (open)

`declawd` is a custom agent the owner added (see [[BPR-1772]]). For a custom
agent, Check it works only proves that its program can be launched and, if
the entry names one, that its own check command exits 0
(`agent_setup_host::check`, owner decision 2026-10-23); it never runs one
turn through the protocol the entry names. A program that launches but does
not speak that protocol, or answers a review without a readable findings
payload, passes the setup, reaches Ready, and then every Ask ends
`protocol_failure` ("answered in a form this app can't read") after the
agent has run (here 13 to 15 seconds). So Ask was not allowed before setup
finished; setup finished on a check that does not exercise what Ask needs.
`protocol_failure` covers several decoder refusals ([[BPR-1827]]):
malformed or incomplete frames, a failed or interrupted remote turn, and a
malformed findings payload among them; the stopped banner's **Show the log**
names which one ("What went wrong: … (name)") and the first line it could not
read. Which one declawd hit is not known here; the owner's log, or that
banner, says. The fix (a protocol round trip in the custom agent's check, or
a check that runs one short turn) is a design change to the setup and is not
in this card.

## Open

- The custom-agent check above.
- The stopped banner reflects the last run while the turn's recovery
  reflects that turn: after an unreadable answer the banner offers Ask again
  and the turn Agent settings…. Both are existing ways back; the owner may
  want one.
- Hiding the suggestions while the Ready line is up is a reading of "before
  the wizard completes" (the Ready line is the setup's fourth step); AC5 asks
  the owner.
- A setup that was abandoned at a failed check and the application reopened
  stands at Check it works with its checks not run, which counts as finished:
  the checks are not kept between runs, so that case cannot be told apart from
  a setup finished before.
