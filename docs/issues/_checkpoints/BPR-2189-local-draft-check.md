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

## Commits

`d0c5774bd` (the check), `a4fcaf5ed` (its hunks in `window_quick_ask.cpp`
and `agent_setup_store.cpp`), `9153284be` (production app manifest pin
`69855406…`, computed with `BPR_PRINT_PINNED_DIGESTS` over `git archive
HEAD src/app`).

## Verification so far

All of AC01 to AC06's courses pass at `72955fd96` (see the card's "Parked on
the owner's look"). Format gate: only `src/application/src/github_window_read_host.cpp`
(BPR-2181's file) fails it. Documentation gate passes with the card in
40-parked.

## Commits since the first checkpoint

`00cb7eb4f` (owner-channel hook count), `0cfc8866e` (plan card toggles wrap;
course captures the side chat and checks the fit), `8891776fc` (report
commands read "Ran `command`, exited N"), `0986111a5` (the look refreshes
through `refresh_local_draft_for_look` and includes its edit), `72955fd96`
(app manifest pin `6e5cfda4…`).

## Pending

- AC07 with the owner: attended session `7a0e21ad68d16a7e91b1c3d0f45f6307`,
  state `/Users/jaeger/.attended-review-state/local-draft-check-v001-2026-11-16/state`,
  cursor 12, frozen root `/Users/jaeger/local-draft-check-v001.hu1xgl14`.
  Next: read `status --after 12`; on four Accepts move BPR-2189 to Done and
  close BPR-2083 and RFC-BPR-0120; on a Reject, change and freeze a v002.
- Incomplete freeze roots the tool will not remove (no marker):
  `/Users/jaeger/local-draft-check-v001.o89_qw_w` and
  `/Users/jaeger/local-draft-check-v001.spoiehlm` (both stopped mid-build;
  the owner or a later round removes them).
