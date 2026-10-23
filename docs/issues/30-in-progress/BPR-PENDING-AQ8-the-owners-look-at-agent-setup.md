---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ8"
title: "A frozen build and the owner's look at agent setup"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ6", "BPR-PENDING-AQ7"]
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-PENDING-AN","BPR-PENDING-AQ4","BPR-PENDING-AQ5"],"milestone":"RFC-BPR-0092 M8","attended_session":{"id":"077adf53047441a3224b3687ceddf38e","state":"/Users/jaeger/.attended-review-state/bpr0092-look-v002","transport":"serve --tailscale","lifecycle":"v002 handed back (Reject) and stopped"},"previous_sessions":["89855cd96f098cd74e28d271acd60d8a (look v001, handed back, stopped, released)"],"frozen_build":{"root":"/Users/jaeger/bpr0092-agent-setup-v002.fwb9a7cz","commit":"f51ae6acf","executable_sha256":"ba4e00b091b96d50baae12381ec20e3fd8ccb82c36e5a3a66f10d61f6e2d2fa9","fixture":"github-com"}}
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
- Handed back 2026-10-23: **Q1 Reject**. The owner's note, verbatim (line
  breaks joined): "Why do I have to click a little circle instead of the whole card to select
  the agent? If I click change on agent it really makes me change it... I
  can't select the one I already have selected or click (2) to continue. The
  add folder thing works althoug I prefer a UX more like the visual studio
  one. This Ask the Agent button is confusing. We have ask below. I don't know
  what this does? Is this button get the agent to review the PR? Or is it get
  it to review the current file? Or is it the same as the ask button below? I
  asked "Does this work?" and it thought for a while and said nothing. It also
  kept my question in the question box even after I had asked it already which
  is weird. Is it not meant to work like the claude code vscode plugin where
  it's like a chat thread? I dunno how this thing is supposed to function I
  guess. I clicked asked the agent this time and it thought for a while and
  then said it found nothing to repair which isn't terribly useful as far as me
  telling if it works or not. Not having a model/effort picker in this window
  bothers the hell out of me. I can't even do `/model`. I really have no clue
  what my agent will be actually doing and that bothers me a lot because the
  quality and cost vary dramatically depending on those things." Their four screenshots showed the agent
  rows' radio circles, Visual Studio's folder list (add, delete, move down,
  move up, an editable path row with a browse button), the "Ask the agent"
  button beside "Ask a question below…", and the tab after "Does this work?":
  "Claude Code finished. It found nothing to report." with the question still
  in the field.
- Look v002's host was stopped, its candidate released and its root removed.
- Owner decisions after look v002, 2026-10-23, in chat (relayed by the
  coordinator), verbatim, recorded in the RFC's
  [Owner decisions (2026-10-23)](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#owner-decisions-2026-10-23-the-conversation):
  (c) "A chip in the message box like VS Code."; (b) "Continuous session per
  PR with an option to clear session."; (a) "Suggested prompts described that
  way sound great." All three forks are answered; look v003 asks only the
  look question.
- Owner decision 2026-10-23, in chat (relayed), on how agent responses become
  notes, verbatim: "Yep build it like that." Finding cards under the answer
  with Add as draft comment, Save as note and Dismiss; Save as note on a plain
  reply; nothing automatic. Recorded in the RFC; look v003 covers it.
- Owner amendment 2026-10-23, in chat (relayed): agents are a list the
  reviewer manages ("I should be able to define more than one custom agent
  ... for the actual settings it show be more like "add agent" where you can
  pick a popular one like claude/codex if they aren't already added or a
  custom one."), and owner decision the same day: "Yeah separate session for
  each agent makes the most sense to me. Obviously anything we save on the PR
  as notes or drafts are outside of that session context." Both recorded in
  the RFC; look v003 covers them.

## Look v003 (2026-10-23)

- Frozen from `336f3f74c` (on main `055bed96e`, with the v002 answers and
  owner decisions (a), (b) and (c), finding cards, the agent list and a
  session per agent); the launcher's own check reached
  `ATTENDED_PULL_REQUEST_OPENED`. Root `/Users/jaeger/bpr0092-agent-setup-v003.onalv1pu`,
  with the stand-in clone under `clones/review-repository`.
- Session `2a87a6833554e6c884a78dbdb02232aa` (fresh, short; state
  `~/.attended-review-state/bpr0092-look-v003`), served with
  `serve --tailscale`; one question, the look:
  - Q1 (AC1, RFC AC14), the rebuilt Agent tab: "The first time I open the
    Agent tab it tells me what to do, and I end up asking a question that
    the agent answers from my repository."
- Waiting on the owner.
