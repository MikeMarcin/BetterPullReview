---
schema: "implement-rfc.card.v1"
id: "BPR-2189"
title: "Plan and run a check on a local draft, and read its report"
status: "Done"
rfcs: ["RFC-BPR-0120"]
depends_on: []
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2189--BPR-2189--1aa8242b9237.json","replaces":"BPR-2082 (closed as rejected with the evidence design)","owner_decision":{"date":"2026-11-15","quote":"Rewrite without evidence"},"owner_review":{"date":"2026-11-15","session":"ab74a8e490f235b4dd24fa66146a9f14","revision":"rfc-0120-rewrite-v001-baf674386","verdicts":"rewrite and Q1 to Q5 accepted at their defaults, no notes"}}
---

# BPR-2189: Plan and run a check on a local draft, and read its report

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Plan a check… in the local draft's whole-change conversation and its plan
card with Run and Discard; the check copy materialized from the candidate,
with RFC-BPR-0104's two grants remembered per draft and agent; the run with
its visible commands and Stop; the report card and the report tab with its
outcome word, commands, failure lines, code references and what was not
covered; Run again on a newer candidate; the built-in prompt
`local-draft.run-check`. Load `frontend-design:frontend-design` before the Qt
work and say so in the report.

## Owner review: accepted (2026-11-15)

The owner directed the rewrite on 2026-11-15 ("Rewrite without evidence") and
then accepted it in attended session `ab74a8e490f235b4dd24fa66146a9f14`
(rfc-0120-rewrite-v001, revision `rfc-0120-rewrite-v001-baf674386`, handed back
at event 31; state under `~/.attended-review-state/rfc-0120-rewrite-v001-2026-11-15`):

| Candidate | Verdict | Note |
|---|---|---|
| The rewrite as a whole | Accept | None |
| Q1: always plan first | Accept | None |
| Q2: no words, the agent chooses | Accept | None |
| Q3: no plan editing; ask again | Accept | None |
| Q4: one check copy per candidate, reused | Accept | None |
| Q5: started from the whole-change conversation only | Accept | None |

That review accepted design text only; AC07 (the look on a frozen build) is
still pending. The card is Ready.

## Intended verification

A fixture agent that plans, passes, fails, stops and changes a file in its
copy, against real temporary worktrees and check copies; the author's
worktree compared byte for byte before and after each run. The owner's look
(AC07) is an attended review on a frozen build with the fixture agent, one
question a candidate.

## In progress (2026-11-15)

Built on the v0.5.0 prerelease lane at `d0c5774bd`, `a4fcaf5ed` and `9153284be`
(app pin); checkpoint `docs/issues/_checkpoints/BPR-2189-local-draft-check.md`
on the lane. AC01 to AC06 have their courses (`core_local_draft_check`,
`application_local_draft_check_desktop_public_path` with the fixture agent);
AC07, the owner look, is not yet frozen.

## Parked on the owner's look (2026-11-16)

AC01 to AC06 pass on the lane: `core_local_draft_check`,
`application_local_draft_check_desktop_public_path` (real window, real Git
worktree, the fixture agent), `tour_cli_*`, `application_writing_delivery`,
`sqlite_workbench_settings_storage` (schema 33),
`qt_settings_window` at four scales, `qt_prompts_page`,
`application_profile_tour_handoff_public_path`, `architecture_boundaries`
and `application_review_round_trip_composition_contract`. Fixed this round:
the owner channel's hook count (`00cb7eb4f`), the plan card overflowing the
side chat (toggles now wrap; the course checks the fit, `0cfc8866e`), report
commands dressed as file references (`8891776fc`), and the look's edit never
becoming candidate 2 (`0986111a5`, pin `72955fd96`).

AC07 is with the owner: session `7a0e21ad68d16a7e91b1c3d0f45f6307`, state
`/Users/jaeger/.attended-review-state/local-draft-check-v001-2026-11-16/state`
(the URL stays in that directory's `serve.log`), cursor 12, host PID 93158
(Tailscale child 93168). Frozen root `/Users/jaeger/local-draft-check-v001.hu1xgl14`
from `72955fd96cf4` (launcher check reached `ATTENDED_LOCAL_DRAFT_CHECK_SHOWN`
with 3 plans and 2 reports, fixture agent only; executable SHA-256
`0e0b4e46bf80…`). Four Accept/Reject questions: `check-plan-card`,
`check-passing-report`, `check-failing-report`, `check-older-candidate`.
BPR-2083 stays open until they are accepted.

## Owner look accepted (2026-11-16)

Session `7a0e21ad68d16a7e91b1c3d0f45f6307` (local-draft-check-v001), handed
back at sequence 21, frozen from `72955fd96cf4`. All four questions
**Accepted**, no notes:

| Candidate | Decision |
|---|---|
| `check-plan-card` | Accept (`c9352517`) |
| `check-passing-report` | Accept (`e9f19161`) |
| `check-failing-report` | Accept (`715a0419`) |
| `check-older-candidate` | Accept (`230ad665`) |

AC07 passes. The review host is stopped and the frozen root removed. Done;
[BPR-2083](BPR-2083-close-rfc-0120.md) closes RFC-BPR-0120.
