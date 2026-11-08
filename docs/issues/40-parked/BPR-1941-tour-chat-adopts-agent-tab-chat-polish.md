---
schema: "implement-rfc.card.v1"
id: "BPR-1941"
title: "Tour chats show the Agent tab's live run status and adopt its chat polish"
status: "Parked"
parked_on: "human-review"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-08, coordinator chat","related_cards":["BPR-1914","BPR-1925","BPR-1934","BPR-1876","BPR-1880"]}
acceptance: [{"criterion": "AC1 A running tour chat turn shows the agent, its current activity and a live elapsed time, the same as the Agent tab (Qt test with a stub run)", "outcome": "pass", "revision": "678230dd6", "evidence": {"kind": "command", "detail": "qt_tour_side_chat: stub run events through review_tour_editor::present_run_status; the turn says making its workspace, thinking, then writing in agent_run_status_words, ticks to 0:01 by itself and keeps its time when redrawn. The frozen look prints ATTENDED_TOUR_CHAT_STATUS ... running=● Claude Code is thinking, 0:03.", "path": "docs/issues/40-parked/BPR-1941-tour-chat-adopts-agent-tab-chat-polish.md#verification-v001-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC2 Each adopted Agent tab behaviour works in tour chats. The card lists what was adopted and what was skipped", "outcome": "pass", "revision": "678230dd6", "evidence": {"kind": "command", "detail": "qt_tour_side_chat: answered in, Stopped after in italics, Resend asks again, Delete dispatches; core_tour_agent_session remove_tour_run; Draft Tour line. Adopted and skipped listed on the card.", "path": "docs/issues/40-parked/BPR-1941-tour-chat-adopts-agent-tab-chat-polish.md#adopted-and-skipped-ac2"}}, {"criterion": "AC3 Tour-specific features are unchanged (existing tests stay green)", "outcome": "pass", "revision": "678230dd6", "evidence": {"kind": "command", "detail": "20 qt_tour_*, qt_review_tour_*, qt_agent_*, core_tour_agent_session and application_tour_agent_conversation tests pass; qt_workbench_window_scale_100 and qt_workbench_agent_send_refusal pass", "path": "docs/issues/40-parked/BPR-1941-tour-chat-adopts-agent-tab-chat-polish.md#verification-v001-focused-owner-dont-wait-for-tests"}}, {"criterion": "AC4 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "678230dd6", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/.codex/attended-builds/bpr1941-tour-chat-status-v001.5w2du995 of 678230dd6; new short session 0ecccbbe9c645b26c26080b97f44808d, candidate BPR-1941-tour-chat-status-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1941-tour-chat-adopts-agent-tab-chat-polish.md#attended-review-v001"}}, {"criterion": "AC5 The documentation gate passes", "outcome": "pass", "revision": "678230dd6", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK", "path": "docs/issues/40-parked/BPR-1941-tour-chat-adopts-agent-tab-chat-polish.md#verification-v001-focused-owner-dont-wait-for-tests"}}]
---

# BPR-1941: Tour chats show the Agent tab's live run status and adopt its chat polish

## Owner (2026-11-08)

"I want to bring the agent status updates from the agent tabs over to the
tour tab's side chat. the '<agent> is thing <elapsed time>' or whatever it
has is much better than the tour side chat's little status disclosure thingy
that has no useful info. There might be more better ux in the agent chat to
bring over, it just feels more polished overall."

## Change

Load `frontend-design:frontend-design` first.

- **Live status.** While a tour chat turn runs (any part's chat or the
  whole-tour chat, and Draft Tour too, where it shows status), show the Agent
  tab's live run status line: the agent's name, what it is doing now (e.g.
  reading or searching), and the elapsed time, updating live. Reuse the Agent
  tab's component and its source of run events. Don't re-implement it.
  Replace the tour chat's "Worked for Ns" disclosure, or keep a completed
  run's steps only if the Agent tab shows them the same way.
- **Audit and adopt.** Compare the Agent tab's chat with the tour chat, and
  adopt what fits. Candidates:
  - turn footers ("<agent> answered in 0:12");
  - failure turns with Resend, Delete and "Fixed since" (BPR-1876 and
    BPR-1880);
  - Stop while running;
  - the run's steps as the Agent tab shows them;
  - message layout and spacing;
  - copy affordances;
  - scrolling behaviour while a turn streams.

  Prefer sharing components over copying. List what you adopted and what you
  left out, with reasons, on the card.
- Keep the tour-specific pieces: "Update … from this chat", proposals in
  place, suggested prompts (BPR-1934), and refusal wording (BPR-1937).

## Acceptance

- AC1. A running tour chat turn shows the agent, its current activity and a
  live elapsed time, the same as the Agent tab (Qt test with a stub run).
- AC2. Each adopted Agent tab behaviour works in tour chats. The card lists
  what was adopted and what was skipped.
- AC3. Tour-specific features are unchanged (existing tests stay green).
- AC4. Owner look: Accept/Reject on a frozen build.
- AC5. The documentation gate passes.

## Implementation v001 (2026-11-08)

Loaded `frontend-design:frontend-design` first. One element carries the
change, the Agent tab's own live line; everything around it stays the tour
chat's quiet column.

- **One shared line.** `agent_run_status_line` and `agent_run_status_words`
  (`src/adapters/qt/include/bpr/qt/agent_run_status.hpp`) are the Agent tab's
  turn ending, moved out of `agent_tab_view` so both surfaces use them. While
  a run is live the line says "● Claude Code is thinking, 0:12." (making its
  workspace, thinking, or writing, plus "It sees only this pull request's
  files." without a clone) and ticks every second by itself from its owner's
  clock, so a chat drawn again keeps the run's time. Ended, it says
  "Claude Code answered in 0:12." or "Stopped after 0:12." in italics. The
  Agent tab's footer is now this line; its words are unchanged.
- **The run's events.** `tour_session_projection` now hears the tour run's
  output events (it dropped them). The host relays admission (making its
  workspace), the workspace (thinking; files only when shallow) and the first
  output (writing) as `qt::agent_run_status` through
  `review_tour_editor::present_run_status`, and clears it when the run ends
  or is refused. Qt decides nothing; it shows what it is told.
- **Turn endings and error turns.** Each ended tour chat turn has one row:
  its ending line, then Copy, and for a turn without an answer Resend and
  Delete, exactly as `core::decide_agent_turn_actions` decides for the Agent
  tab (Resend on the latest turn while nothing runs; Delete on every error
  while nothing runs). A failed run is `exited_with_error`, a stopped one
  `stopped`. Resend asks the same question, passage and Update flag again in
  the same chat. Delete is the new `core::remove_tour_run`, which removes only
  an ended, unanswered ask turn. Copy on an error copies the failure and its
  log. Copy chat names the agent ("**Claude Code** (answered in 0:12):").
- **Draft Tour.** While it runs, the same live line shows under the Tour
  tab's notice (`tourDraftLiveStatus`) and, under a "Draft Tour" heading, at
  the end of the whole tour's chat.

### Adopted and skipped (AC2)

Adopted: the live run line (agent, activity, elapsed time); turn footers
("answered in", "Stopped after"); failure turns with Resend and Delete, from
the core's decision; Copy in the turn's ending row, also on errors; Stop while
running (the chat's Send was already Stop, as the Agent tab's is).

Skipped, with reasons:
- **The run's steps.** The Agent tab shows no steps, so the folded
  "Worked for Ns" and its steps left the chat (the card: keep them only if the
  Agent tab shows them the same way). The conversation window keeps every
  run's steps.
- **"Reading" or "searching" as the activity.** Neither surface can say it
  live: the protocol loop gives tool uses to the tour session only with the
  run's result, and streams only output text. Both say making its workspace,
  thinking or writing. Streaming tool activity would be a protocol-loop and
  host change of its own.
- **"Fixed since" (BPR-1876).** It names a setup check that has passed since
  a start or workspace failure; a tour run's failure names no such check.
- **Message layout and spacing, the red error card, turn times.** The tour
  chat's bubbles and left-aligned replies are the owner-approved column
  (BPR-1914 v003, BPR-1925) and suit its narrow width; only the one-row
  ending was adopted. Tour runs keep no asked or answered time.
- **Scrolling while a turn streams.** A tour reply arrives whole, not
  streamed; the chat already keeps its end in view as a turn is added.

Tour-specific pieces are unchanged: "Update … from this chat", proposals in
place, the suggested prompts (BPR-1934), refusal wording (BPR-1937) and the
focus line.

- **Look.** `--attended-tour-drafting --attended-tour-chat-status` (with the
  `attended-drafting-claude` stub) drafts and accepts the tour, then asks three
  questions in the Solution's chat: one answered, one the stub fails ("Does
  this hold under load?"), and one it thinks over for ten minutes ("Walk me
  through every caller."). It prints `ATTENDED_TOUR_CHAT_STATUS:…` and
  `ATTENDED_TOUR_CHAT_STATUS_SHOWN` and leaves that turn running, for the
  person to watch and Stop.

### Verification v001 (focused, owner: "dont wait for tests")

Dev build, macOS:
- `core_tour_agent_session` (`remove_tour_run`), `qt_tour_side_chat` (AC1:
  the running turn says "● Claude Code is making its workspace…", then
  "thinking", ticks to 0:01 by itself, then "writing", and keeps its time
  when redrawn; AC2: "Claude Code answered in 0:07.", a failed turn's
  "Stopped after 0:03." in italics with Resend, which asks again, and Delete,
  which dispatches; Draft Tour's line in the tour chat and under the notice),
  and the 18 other `qt_tour_*`, `qt_review_tour_*`, `qt_agent_*` and
  `application_tour_agent_conversation` tests pass (AC3).
- `qt_workbench_window_scale_100` (the Agent tab's turn footer words,
  unchanged, on the shared line) and `qt_workbench_agent_send_refusal` pass.
- `application_review_round_trip_composition_contract` passes, its app
  source pin resealed from its own output (`ab83c7ed…`).
- `coding-format-check` passes (clang-format 22.1.8);
  `verify_documentation.py` prints `DOCUMENTATION_OK` (AC5).
- The offscreen look on the dev build, light and dark, prints
  `ATTENDED_TOUR_CHAT_STATUS:answered=Claude Code answered in 0:03.:failed=Stopped after 0:03.:running=● Claude Code is thinking, 0:03.`
  then `ATTENDED_TOUR_CHAT_STATUS_SHOWN`.

### Attended review (v001)

- Frozen build `/Users/jaeger/.codex/attended-builds/bpr1941-tour-chat-status-v001.5w2du995`
  of `678230dd6` (executable SHA-256
  `581314d5c0bcdd6f5db3a86ea97d81f19ab7c400bba002713d195a34858117fd`). Its
  launcher's own offscreen check reached `ATTENDED_TOUR_CHAT_STATUS_SHOWN`.
  The frozen executable, offscreen, light and dark, prints
  `ATTENDED_TOUR_CHAT_STATUS:answered=Claude Code answered in 0:02.:failed=Stopped after 0:02.:running=● Claude Code is thinking, 0:03.`;
  the pictures in the session are its own renders.
- Launch: `python3 /Users/jaeger/.codex/attended-builds/bpr1941-tour-chat-status-v001.5w2du995/launch.py`.
- New short session `0ecccbbe9c645b26c26080b97f44808d`, state
  `/Users/jaeger/.codex/attended-review/bpr1941-v001-tCIzL4/state`, host
  started with `serve --tailscale`.
- One candidate, `BPR-1941-tour-chat-status-v001` (revision
  `678230dd6-v001`), with one Accept/Reject question.
- Status: ready, awaiting the owner. Branch `worktree-agent-acca666ced3cb9c6e`,
  not pushed or merged.
