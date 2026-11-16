# BPR-2189 checkpoint: plan and run a check on a local draft

Lane `claude/v0-5-0-prerelease-targets-52265d`, base `6c8f08694`. Assignment:
build BPR-2189 (RFC-BPR-0120 as rewritten without evidence; owner Q1 to Q5
accepted at their defaults), then close BPR-2083 and RFC-BPR-0120 when every
criterion has evidence. RFC-BPR-0109 (evidence) is Rejected; nothing here
stores, cites or reintroduces evidence.

## Design as built

- Core `local_draft_check` (`src/core/include/bpr/core/local_draft_check.hpp`):
  the plan an answer's `bpr-check-plan` block holds; `decide_check_run` and
  its reasons (naming Use scratch folder or Allow build and run tests,
  another check running, no candidate); the report from the answer's
  `bpr-check-report` block and what the host saw (activity commands, files
  changed in the copy, Stop); the outcome words; the report's Markdown; the
  plan and run requests. A turn of a draft's conversation carries an
  optional `local_check` (stored as JSON in the turn); the preparation keeps
  each agent's two grants. `append_check_turns` keeps a report with the
  conversation it was planned in, whichever candidate it is on, and lets the
  conversation's resumed session go so a later question replays the report.
- Application: `local_check_copies` (one check copy per candidate under the
  agents' scratch root, key `check-DRAFT-N`, `copy/`, `scratch/`,
  `manifest`), written on the draft host's effect lane from the candidate's
  head tree (isolated Git reader) and the included working files' captured
  bytes (the store), never the worktree; reused (Q4); older ones pruned when
  no check uses them; removed with the draft; bounded by the scratch folder
  limit. `local_draft_workbench::plan_check`, `run_check` (Run and Run
  again), `discard_check`, `set_check_grants`; live activity through
  `publish_check_activity`.
- App runner (`window_tours.cpp`): a plan runs with read-only tools, a run
  with the draft's grants (`agent_launch_grant`), both in the copy with its
  own agent settings shut out (`agent_checkout_spellings`), fresh
  (`assistance_first_answer`); the activity sink lists the commands.
  `writing_stop` keeps a running check from being stopped when the window
  loses focus.
- Prompt `local-draft.run-check` (`writing_hook::local_run_check`,
  `resources/review-preparation/local-run-check.md`, schema 33 column
  `writing_local_run_check`, hook tables 0 to 15), listed under Prompts ›
  Local drafts.
- Qt: `local_check_card` (plan card: numbered commands, what each shows,
  Won't cover, the two toggles, Run with its reason, Discard; report card:
  outcome word, candidate words, tally, Open report, Run again) inside the
  Tour panel's side chat; Plan a check… beside Session in the whole-change
  conversation. Reports open in a file tab through `open_report`; their
  `bpr-check:PATH:LINE` links open the checked candidate at the line.
- Fixture agent: plans from the author's words ("fail", "touch", "scratch")
  and runs the plan as Bash tool calls in its working folder; "touch" really
  rewrites the copy's first file.

## Verification so far

- `core_local_draft_check` (new) and `core_local_draft_tour` pass.
- `application_local_draft_check_desktop_public_path` (new; real window and
  composition, real Git worktree, the fixture agent binary) was green
  through AC01, AC02, AC03 and most of AC04 at the last run; the remaining
  assertions were being fixed (report counting across conversations).
- `frontend-design:frontend-design` loaded before the Qt work.

## Pending

- Finish the desktop course; run the affected suites (settings storage
  schema 33, prompts panel, writing delivery, agent configuration sharing,
  architecture boundaries, the tour desktop course); format; reseal the app
  source pin (`bpr_expected_production_app_manifest_digest` in
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`)
  over this commit's tree only, since other implementers' src/app edits are
  uncommitted in the same worktree.
- Commit explicit paths and only this card's hunks (`window_quick_ask.cpp`,
  `agent_setup_store.cpp` and others carry other lanes' changes).
- AC07: freeze with `--attended-local-draft PATH --attended-local-draft-check`
  (scripted through the side chat with the Fixture agent; ready line
  `ATTENDED_LOCAL_DRAFT_CHECK_SHOWN`), generated worktree
  `tests/fixtures/local-draft/make_worktree.sh`; one Accept/Reject each for
  the plan card, a passing report, a failing report, and a report on an
  older candidate; serve on the tailnet; record the session, not the URL.
- Tracker work in the primary clone on `main`: BPR-2189 to Human Review
  with the look parked; BPR-2083 waits on AC07.
