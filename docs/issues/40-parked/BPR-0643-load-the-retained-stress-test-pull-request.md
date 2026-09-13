---
schema: "implement-rfc.card.v1"
id: "BPR-0643"
title: "Load the retained stress-test pull request"
status: "Parked"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0642"]
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
parked_on: "human-review"
---

# Load the retained stress-test pull request

## Current frontier — bpr0643-r1 / d1 / mandate r1

The bound `retained-stress-pr-read` milestone targets only a read-only Mac load
of the owner's retained `EpicGames/UnrealEngine#1773` through an already
configured account, if available. BPR-0642 is Done and supplies structured
`github.pull_request` diagnostics. The next discriminating step is to establish
the locally configured account and run the candidate's read path, retaining
the exact failed diagnostic log outside the repository. If account read authority
is unavailable, use local fixtures to complete independent product diagnosis
and leave the live criterion open. No live write, publication, credential
exposure, or private PR content retention is authorized.

The authenticated `gh` CLI session reports the owner's `MikeMarcin` account
and read-only GET confirms the target is open with 1,523 changed files and
9,311 total commits. The product cannot adopt CLI credentials; its sole
persisted native device binding is tombstoned, so an accepted product session
is not available. No target load ran and there is no new target failure log or
failure-class observation. The network-isolated dev courses
`application_github_window_read_host`, `github_paged_read`,
`qt_composed_github_scale_100`, `application_github_public_path_github_com`
and `application_logging` pass 5/5 at the current source. The pager's fixture
accepts 3,000 files and 250 returned commits, but this cannot establish the
target's runtime outcome. A task-launched dev UI process was stopped after its
computer-use attachment timed out; it produced no new product diagnostic.
The fresh tests-disabled Release candidate builds at
`build/bpr0643-release/src/app/BetterPullReview.app` from this source. Its
ad-hoc local signature passes `codesign --verify --deep --strict`; direct Mac
launch rendered the workbench, GitHub.com Native HTTPS profile and Accounts &
hosts page, which said `No account` and `Signed out`. The task-owned candidate
process exited after the inspection. The remaining live step requires the
owner to establish a native HTTPS product session as `MikeMarcin` in this
candidate, then re-run this exact read-only target and inspect its per-attempt
diagnostic class, file list and selected diff. The owner may need to seal the
local candidate with the existing trusted signing identity before device
sign-in; this dispatch did not use signing credentials.

This card is claimed on main; only its live record and bounded product/test
paths are owned. Existing `.gitignore` and `.mask.json` edits are unrelated.

The owner retained `EpicGames/UnrealEngine#1773` as a stress-test target after
the signed private alpha candidate reported `The pull request could not be
loaded.` for it while connected as the owner's account. It is an open 2015
pull request from a fork with 9311 commits (GitHub lists only the newest 250),
1523 changed files, 111 issue comments and 19 review threads. Read-only
inspection against the read model at `50f364b8` found no bound, nullable
field, status value, duplicate identity or GraphQL error that explains the
refusal; the observation is on
[BPR-0641](BPR-0641-enable-normal-inline-publication-for-private-alpha.md).

## Scope

After BPR-0642 records load outcomes, reproduce the failure with the owner's
account, read the failure class from the diagnostics log, and either lift the
limit that refuses this shape or make the refusal explicit in the session
panel with the reason. Reading only; publication to it is not in scope. Do not
retain the pull request's content in the repository.

## Acceptance

- The diagnostics log names the failure class for this target.
- Either the target opens read-only in the Mac candidate with correct account,
  file list and a readable selected diff, or the refusal is presented with its
  reason and recorded here as a known limit.
- No live write occurs.
