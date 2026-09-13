---
schema: "implement-rfc.card.v1"
id: "BPR-0643"
title: "Load the retained stress-test pull request"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0642"]
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
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
[BPR-0641](../40-parked/BPR-0641-enable-normal-inline-publication-for-private-alpha.md).

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
