---
schema: "implement-rfc.card.v1"
id: "BPR-0966"
title: "Reconcile Dif workshop cues with the dedicated composition page"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"origin_rfc":"RFC-BPR-0031","related_rfc":"RFC-BPR-0037","found_by":"BPR-0507 owner-amended closeout","mandate_revision":"r4","found_at":"9479f1c3a7bd9e849e11fc64d3c156d0ede2a58d","scope":"Standalone later-UI contract follow-up; no RFC0012 product criterion is waived and no completed RFC0031 acceptance is rewritten","milestone_plan":{"id":"bpr0966-dif-cues","path":"docs/rfcs/plans/bpr0966-dif-cues/milestone-plan.json"}}
---

# Reconcile Dif workshop cues with the dedicated composition page

## Observation

On 2026-09-21, the RFC0012 closeout's broad agent selector reached
`application_agent_finding_dif_context_contract` and failed only
`dif-comment-ready-after-conversion` and `dif-acknowledge-after-acceptance`.
The exact diagnostic was `agent-finding-dif-context-evidence:1:1:1:0:0:1:0`.
The same failure repeats when the course runs alone (18.13 seconds):

```sh
ctest --test-dir /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev --output-on-failure --no-tests=error -R '^application_agent_finding_dif_context_contract$'
```

Product/test candidate: `9479f1c3a7bd9e849e11fc64d3c156d0ede2a58d`, unchanged
through the documentation-only claim at `abd9baee670332b2c6fefd17d01ab6a84eabe9b3`.
Finding-course executable SHA-256:
`c26423eb82a5e676b681c3d31ca2187a61602c376a46522a9f393d9ab9c4b1ee`.
App executable SHA-256:
`a0f9c85cd58e4c9d486ae39c554b16f30f4bb73e867ebfbb6c17ad21ca775d90`.

The same test confirms comparison, consider and attention cues, one queued
finding, the exact proposed and accepted draft bytes, and zero GitHub mutation.
The original finding/chat/workshop courses pass. This is a failing later Dif
presentation contract, not evidence of a failed draft conversion or a passing
Dif course. No native observation or new human decision was obtained.

## Source explanation and remaining judgment

RFC0037 integration `cc03874861abd658cce11812f15d887607919d1b` added
`Window->open_review_composition(true)` before `present_dif_workshop` in
`github_application::present_finding_outcome`. That opens the dedicated
composition page. `workbench_window::apply_drawer_layout` then hides every
drawer while that page is active; `showing_dif_ai` requires a visible
conversations drawer, and `update_dif_presentation` disables the cue router
and hides the illustration when that condition is false. The course still
expects `comment_ready` and `acknowledge` immediately after conversion and
acceptance. This source path explains the observed mismatch; no first-bad
bisect or control build was run, and whether the course or product must change
requires reconciliation with the approved RFC0031/RFC0037 presentation contracts.

## Acceptance

- AC1: Resolve the workshop cue expectation against the dedicated editor page,
  documenting the approved behavior and any required owner amendment without
  silently rewriting terminal RFC0031 acceptance. Coordinate with BPR-0800's
  separate placement decision if the chosen behavior affects it.
- AC2: Correct the identified stale expectation or product regression with a
  focused failing-before check; preserve exact session/action/draft identity,
  single-sprite ownership, explicit acceptance and zero publication effects.
- AC3: Run the Dif context course and the affected router/editor/public
  composition checks at the exact candidate. Record native or human decisions
  separately if the resolved presentation contract requires them.

The r4 owner closeout leaves this as an independent follow-up. BPR-0507 does
not claim these two cues passed and does not modify their assertion or source.

## Windows also misses the consider cue (bpr-main-win, 2026-09-24)

Folded in by the coordinator as the reserved `dif-saw-consider` finding; this
card is not moved or claimed by it. Measured on the bpr-main-win station
(`build/windows-dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial), and
recorded first on [[BPR-1050]]. The logs are station records under
`build/logs/bpr-1050-main-win/` (`difctx-*.log`, `rows32.log`, `sweep.log`)
and are not committed.

On Windows the course fails one assertion more than the macOS observation
above. As well as this card's two, it misses the consider cue:

```text
agent-finding-composition-failure:dif-context-evidence failed=dif-saw-consider dif-comment-ready-after-conversion dif-acknowledge-after-acceptance
agent-finding-dif-context-evidence:1:0:1:0:0:1:0
```

The second field is `dif-saw-consider`; macOS printed `1:1:1:0:0:1:0`.

| Candidate | Runs | Missed consider |
| --- | --- | --- |
| `bcd57840` | 9: 7 alone, 1 in the 32-row table, 1 in the 154-row `^application_` sweep | 9 of 9 |
| `bcd57840` with only `tests/end_to_end/agent_finding_composition_public_path.cpp` put back to `3db65162`, the version before [[BPR-1050]]'s `ff9bc9e2`, and its target rebuilt | 6 alone | 4 of 6; the other 2 printed the macOS shape, `1:1:1:0:0:1:0` |

So the miss predates BPR-1050's data-root change. Nine of nine after it
against four of six before is too few runs to say whether the change made it
more frequent. The course does not wait for the consider cue.
It samples Dif's current clip on each pass of `wait_until_reporting` while
the agent turn runs, and `difSawConsider` becomes true only if a pass happens
to land while the consider clip is showing. A miss means no pass saw it: the
clip was shown between passes, or not shown at all. These runs do not tell
the two apart. The attention cue after delivery (the third field) was seen
in every run, and no run printed a refusal line.

For AC2 and AC3: when this card reconciles the workshop cues, the consider
assertion should either record the clip when Dif shows it, rather than sample
it, or be named as a timing-dependent observation. Its Windows outcome is
wanted in AC3's run at the exact candidate.
