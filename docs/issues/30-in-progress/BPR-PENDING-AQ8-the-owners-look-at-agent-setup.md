---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ8"
title: "A frozen build and the owner's look at agent setup"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ6", "BPR-PENDING-AQ7"]
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-PENDING-AN","BPR-PENDING-AQ4","BPR-PENDING-AQ5"],"milestone":"RFC-BPR-0092 M8","attended_session":{"id":"89855cd96f098cd74e28d271acd60d8a","state":"/Users/jaeger/.attended-review-state/bpr0092-look-v001","transport":"serve --tailscale","lifecycle":"serving, awaiting the owner"},"frozen_build":{"root":"/Users/jaeger/bpr0092-agent-setup-v001.s4c2jv_v","commit":"f73f94770","executable_sha256":"4aaf39ef72dfd5668cbe01e2809d4dbf5eeddd5acce837d64f837234aa786c0a","fixture":"github-com"}}
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
- Pending: all three answers.
