---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ8"
title: "A frozen build and the owner's look at agent setup"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ6", "BPR-PENDING-AQ7"]
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-PENDING-AN","BPR-PENDING-AQ4","BPR-PENDING-AQ5"],"milestone":"RFC-BPR-0092 M8","attended_session":{"id":"077adf53047441a3224b3687ceddf38e","state":"/Users/jaeger/.attended-review-state/bpr0092-look-v002","transport":"serve --tailscale","lifecycle":"serving look v002, awaiting the owner"},"previous_sessions":["89855cd96f098cd74e28d271acd60d8a (look v001, handed back, stopped, released)"],"frozen_build":{"root":"/Users/jaeger/bpr0092-agent-setup-v002.fwb9a7cz","commit":"f51ae6acf","executable_sha256":"ba4e00b091b96d50baae12381ec20e3fd8ccb82c36e5a3a66f10d61f6e2d2fa9","fixture":"github-com"}}
---

# BPR-PENDING-AQ8: A frozen build and the owner's look at agent setup

RFC-BPR-0092 M8 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)).

## Scope

- A frozen build with `scripts/freeze_attended_build.py`, a fresh short
  attended session served with `serve --tailscale`, one question per
  criterion, and the questions also in the report while the page's
  Accept/Reject bug stands.

## Acceptance criteria

- AC1: RFC AC14, the owner's own words recorded with the session.

## Look v001 (2026-10-22)

- Frozen with `scripts/freeze_attended_build.py freeze --name
  bpr0092-agent-setup-v001 --fixture github-com --open-pull-request` from
  `f73f94770`; the launcher's own check reached
  `ATTENDED_PULL_REQUEST_OPENED`. The look opens octo-org/review-repository
  #17 with a fresh profile; an attended launch looks for this Mac's own
  Claude Code and Codex ([[BPR-PENDING-AQ8]]'s commit), every automated course
  keeps its own search path. A stand-in clone of the fixture repository
  (a Git directory with its `origin`, no history) is in the root's
  `clones/review-repository` for the Repository step.
- Session `89855cd96f098cd74e28d271acd60d8a` (fresh, short), served with
  `serve --tailscale`; the capability URL stays in the external session
  state. One question per criterion:
  - Q1 (AC1, RFC AC14): "The first time I open the Agent tab it tells me what
    to do, and I end up asking a question that the agent answers from my
    repository."
  - Q2 ([[BPR-PENDING-AQ4]]'s deviation): keep the workspace borrowing the
    linked clone's history instead of a `git worktree` of it.
  - Q3 ([[BPR-PENDING-AQ5]]'s deviation): keep the edited skill in the
    profile's settings store, edited in Settings, without Open in editor
    for now.
- Handed back 2026-10-22: **Q2 Accept**, **Q3 Accept**, **Q1 Reject**. The
  owner's note on Q1, verbatim: "I would prefer if add a folder was a text
  entry first with a file browser option on the text line. Way too much
  detail after check if it works.. all of that except the Claude code is
  ready line should be in a collapsed disclosure. I get to here and see no
  indication anything happened." Their screenshots showed the ready card
  with its details open, and the working banner at "Approved Claude stream
  JSON is working / Preparing agent invocation" with no further change.
  [[BPR-PENDING-AQ6]]'s follow-up commit answers each point; look v002
  asks Q1 again.

## Look v002 (2026-10-22)

- Look v001's host was stopped, its candidates released and its frozen root
  removed with `freeze_attended_build.py remove`.
- Frozen from `f51ae6acf` (rebased onto main `b8d2ec6cf`, with
  [[BPR-PENDING-AQ6]]'s answer to the v001 note); the launcher's own check
  reached `ATTENDED_PULL_REQUEST_OPENED`. The root's `clones/review-repository`
  is the same stand-in clone.
- Session `077adf53047441a3224b3687ceddf38e` (fresh, short), served with
  `serve --tailscale`; one question:
  - Q1 (AC1, RFC AC14), the revised first run: "The first time I open the
    Agent tab it tells me what to do, and I end up asking a question that the
    agent answers from my repository."
- Pending: the owner's answer.
