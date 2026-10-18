# BPR-1594 handoff: Agent is a drawer tab (in progress)

- Worktree: `/Users/jaeger/.codex/worktrees/bpr1594-agent-tab/BetterPullReview`,
  branch `bpr-1594-agent-tab` (pushed to origin), branched from
  `bpr-1593-tours` at `202e6b3d3`, because it builds on BPR-1592/1593's
  provisional Agent tab. BPR-1593 was accepted; Codex lands it (item 7c of
  `docs/issues/_checkpoints/codex-handoff-2026-10-17.md`) from
  `/Users/jaeger/.codex/worktrees/bpr1574-drawer-redesign`: do not touch that
  worktree, its server or its frozen build. Merge `origin/main` here once
  BPR-1593 is on `main`.
- Build tree: `build/dev` in this worktree (configured with the shared CPM
  cache symlinked from the main checkout).
- Scope (RFC-BPR-0085 D5, AC07): the Agent tab after Tour; ⌥⇧A opens the
  drawer on it; ⌘I and the code's context menu "Ask about these lines" open
  it with the selected lines attached; context row (profile, head, state,
  scope); conversation with finding cards (promote reads Draft comment, Ask
  follow-up, Add to notes, More: Draft a defect, Dismiss); question field with
  context chips; History menu (sessions, earlier heads, private runs); four
  states: not set up, working (Stop), head moved (Start a new session,
  Continue on the old head), stopped (what happened, Ask again, Show the log).
  Read-only authority unchanged.
- Done (all pushed on `bpr-1594-agent-tab`):
  - `src/adapters/qt/{include/bpr/qt,src}/agent_tab_view.*`: the tab. It
    borrows the panels' controls (profile combo, scope combo, replies list,
    composer) and returns them in its destructor; `Dying` and QPointer guards
    stop refreshes during window teardown. Finding cards fold Ask follow-up
    and Add to notes into More when the drawer is narrow (`fit_card_actions`
    against the scroll viewport's width). No style sheets (the principles
    check refuses them): `menu_word` paints a menu button without its arrow.
  - Window: Agent tray tab, ⌥⇧A (`open_agent`), ⌘I (`ask_agent`), code menu
    "Ask about these lines", `show_agent(bool)`.
  - Shortcut catalogue: open_agent, ask_agent, new_note, plus the sheet and
    tour page keys BPR-1590/1593 had not registered.
  - Side chat scopes read in words (Selected lines, This file, Pull request,
    Repository).
  - `src/app/main.cpp`: `--attended-agent-look PATH` (Codex JSONL fixture
    profile, starts a review, prints `ATTENDED_AGENT_SHOWN:findings=N`).
  - Tests: `test_the_agent_is_a_drawer_tab` and adapted drawer tests in
    `tests/adapters_qt/workbench_window_tests.cpp`; the dif context course
    expands the review sheet. Round-trip contract resealed.
  - `docs/ux/workbench-specification.md` describes the tab. Card claimed
    (`docs/issues/30-in-progress/`).
- Verified: qt_workbench_window scales 100/125/200, qt_keyboard_shortcuts,
  qt_side_chat_panel pass. Composition contracts, agent Qt tests, format and
  architecture checks: see the latest commit message or rerun
  `ctest -R "composition_contract|agent_finding_composition|qt_agent|qt_finding_queue|qt_side_chat|qt_private_notes|qt_keyboard_shortcuts|architecture_boundaries|coding_format_check"`
  in `build/dev`.
- Known gaps in v001 (said in the review): finding lines are not washed amber
  and have no gutter mark; History's private history page keeps its legacy
  layout; the head-moved banner has no test.
- Next: freeze `bpr1594-agent-tab-v001` (`scripts/freeze_attended_build.py
  freeze --name bpr1594-agent-tab-v001 --fixture conventional-comments-v1
  --open-file src/engine/renderer.cpp --commit <sha> --target
  better_pull_review_agent_finding_canary --argument=--attended-agent-look
  "--argument={build}/tests/end_to_end/better_pull_review_agent_finding_canary"
  --ready ATTENDED_AGENT_SHOWN --wait 300 --generate "mkdir -p build && ln -s
  /Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
  build/.cpm-cache"`), then a new attended session in
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1594-agent-tab-v001`
  with one Accept/Reject: "The agent has its own tab and is one key away."
  On Accept: merge `origin/main` (after BPR-1593 lands), focused tests and
  composition contracts, reseal, docs gate, `issue_board.py finish`, update
  RFC-BPR-0085's status, push; then BPR-1603's design note and review.
- Open design questions: none.
- Running PIDs: none recorded yet. Review session: none yet.
