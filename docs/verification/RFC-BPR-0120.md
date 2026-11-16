# RFC-BPR-0120 verification

[RFC-BPR-0120](../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md),
as rewritten without evidence (owner, 2026-11-15: "Rewrite without
evidence"; rewrite and Q1 to Q5 accepted at their defaults in session
`ab74a8e4`). Plan `agent-assistance-v1`, milestone `agent-run-checks`. Lane
`claude/v0-5-0-prerelease-targets-52265d`; RFC base
`775bd2d764e1651750d06e2ea798ea4d66934996`. Implementation
[BPR-2189](../issues/60-done/BPR-2189-plan-and-run-a-check-on-a-local-draft.md),
closeout [BPR-2083](../issues/60-done/BPR-2083-close-rfc-0120.md).
Frontend-design was loaded before the Qt work. Nothing here stores, cites or
reintroduces the rejected evidence design (RFC-BPR-0109).

## What was built

| Layer | Delivered |
|---|---|
| Core | `local_draft_check`: the plan from an answer's `bpr-check-plan` block, `decide_check_run` and its reasons, the report from `bpr-check-report` and what the host saw, outcome words, report Markdown, plan and run requests; a turn's optional `local_check`; per-agent check grants |
| Application | `local_check_copies` (one copy per candidate under the agents' scratch root, from the head tree and included files' captured bytes, reused, pruned, removed with the draft, bounded by the scratch limit); `plan_check`, `run_check`, `discard_check`, `set_check_grants`; live activity |
| App | The runner (plans read-only, runs with the draft's grants, in the copy, fresh); prompt `local-draft.run-check` (schema 33); a check survives the window losing focus; the attended look `--attended-local-draft-check` |
| Qt | The plan card and report card in the Tour panel's side chat, Plan a check… beside Session; reports in a file tab whose links open the checked candidate |

Commits: `d0c5774bd`, `a4fcaf5ed`, `9153284be`, `00cb7eb4f`, `0cfc8866e`,
`8891776fc`, `0986111a5`, `72955fd96` (app manifest pin `6e5cfda4…`).

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `core_local_draft_check`; `application_local_draft_check_desktop_public_path` (real window and composition, real Git worktree, the fixture agent): plans with and without words, nothing before Run, Run unavailable with its reason, the card fits the side chat | Pass |
| AC02 | The course: the run uses the candidate's check copy; worktree, branch and index byte-identical; a commit during the run does not change what is checked | Pass |
| AC03 | `core_local_draft_check` and the course: the report tab's outcome, candidate, commands with exit and out-of-plan marks, failure output, a code reference opening the checked candidate, Not covered | Pass |
| AC04 | The course: Passed, Failed, Run again, Stopped and Didn't run all kept; changed tracked files listed; an older candidate's report says so; Run again uses the same plan on the latest candidate | Pass |
| AC05 | The course: reports survive a restart and a follow-up question reads them; nothing reaches description, tour or preview; the scratch limit refuses a copy; copies go with the draft | Pass |
| AC06 | The course (the check prompt reaches plans and runs and no other run); `sqlite_workbench_settings_storage` (schema 33); `tour_cli_*`; `qt_settings_window` at four scales; `qt_prompts_page`; `application_profile_tour_handoff_public_path` | Pass |
| AC07 | Owner look local-draft-check-v001, session `7a0e21ad68d16a7e91b1c3d0f45f6307`, build frozen from `72955fd96cf4` with the fixture agent (launcher check `ATTENDED_LOCAL_DRAFT_CHECK_SHOWN`, 3 plans, 2 reports, no real agent started). Accepted 2026-11-16, no notes: plan card (`c9352517`), passing report (`e9f19161`), failing report (`715a0419`), report on an older candidate (`230ad665`) | Pass (owner) |

Also green at `72955fd96`: `architecture_boundaries`,
`application_review_round_trip_composition_contract`,
`application_writing_delivery`, `qt_agent_configuration_dialogs`,
`sqlite_agent_configuration_storage`, `core_agent_configuration`,
`core_local_draft_tour`. The documentation gate passes.

## Not done, and why

- BPR-2083's intended single run with an installed agent on a real local
  draft was not made. No criterion asks for it, and looks never spend the
  owner's agent usage (owner, 2026-11-14).
- The format gate fails on `src/application/src/github_window_read_host.cpp`
  only, another card's file; every file of this RFC is formatted.

## Observed, not judged

The native renders draw the report cards' outcome words without their
outcome colours, the report tab stays light in Dark, and the Tour drawer's
main-window render shows fragments at its right edge. The owner accepted the
look as shown; these are left for later cards.
