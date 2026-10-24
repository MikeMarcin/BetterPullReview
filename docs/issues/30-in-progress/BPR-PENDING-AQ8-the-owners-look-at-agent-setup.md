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
- Handed back 2026-10-23: **Q1 Reject**. The owner's note, verbatim (line
  breaks joined): "Great progress. It actually functions now. Just a bit of
  cleanup left to do. The drawer setup wizard doesnt need the full folder list
  editor inline there, it's fine to just add a single folder line from there.
  The list editor was meant for settings page. This bottom menu area is a big
  mess. We should try to syntax highlight these responses using our existing
  syntax highlighting for code blocks from the markdown renderer. The note we
  saved also has some not great ux at the bottom. For the chat responses I
  would like it if we had the response contained in something we could copy
  the whole response sort of like the claude and codex app buttons at the
  bottom of the response. Timestamp would also be nice there." Their four
  screenshots showed the question field's bottom row (the chip and
  Suggestions squeezed to "Clau…fort" and "Sug…ons", "No adde" and "Contex"
  cut off beside Ask), an answer with plain code, the saved private note's
  footer ("Only you", delete, a clipped "Draft comment" and Save), and the
  Claude app's answer footer (copy, a menu and "10:03 AM").
- Look v003's host was stopped, its candidate released and its root removed.
- Owner decisions 2026-10-23, in chat (relayed), verbatim: "Don't have a
  timeout limit, cancel is sufficient.", then "I guess we can keep a timeout
  as an option, but i wouldnt want it on by default for my work." Recorded in
  the RFC; v004 builds the second.
- Owner decision 2026-10-23, in chat (relayed), verbatim: "Go with your
  recommendation for the 3 decisions" (the old Claude wrapper settings onto
  the agent list, a custom agent's setup check, and drafting on another
  file). Recorded in the RFC; v004 builds all three.

## Look v004 (2026-10-23)

- Frozen from `c185e0460` (on main `055bed96e`, with look v003's cleanup:
  setup's one folder line, the rebuilt question field, header and thread,
  answer cards with Copy and their time, coloured code, the note footer; the
  optional run limit, off by default; and the three decisions); the
  launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`. Root
  `/Users/jaeger/bpr0092-agent-setup-v004.w6gdtauf`, with the stand-in clone
  under `clones/review-repository`.
- Session `f19bb0862de7d2c398f3845d84868120` (fresh, short; state
  `~/.attended-review-state/bpr0092-look-v004`), served with
  `serve --tailscale`; one question, the look:
  - Q1 (AC1, RFC AC14), the Agent tab after the v003 notes: "The first time
    I open the Agent tab it tells me what to do, and I end up asking a
    question that the agent answers from my repository."
- Handed back 2026-10-23: **Q1 Reject** ("much closer but not quite there",
  relayed). The owner's note, verbatim (line breaks joined; the pasted
  sample answer kept as its first quotation): "Not drawing inline comment
  with syntax highlighing. [I'll read line 34 and the surrounding code to
  explain what it does. Line 34 returns `diagnostic::invalid_calibration`
  when `validate(scale, bias, gamma)` fails. This immediately exits the
  enclosing function, signaling invalid calibration and skipping
  `finalize(scale, bias, gamma)` on line 36. The surrounding snippet doesn't
  show which values validation accepts or rejects.] After save as note it
  should turn into Go to note The models aren't listing their version
  numbers. I would like the suggestions to come back where they were in the
  chat window... maybe turn the suggestion prompts into a toggle to show/hide
  them instead of a drop down. My suggestion question : `Explain the selected
  line (34)` This should be a link to the file and line I can click... in an
  hour I wont remember which file line 34 was in. get rid of the or add it's
  just confusing. Just let the add button be clickable with the the prefilled
  text". Their screenshots showed the answer card (inline code plain, its last
  sentence cut off, "Saved as a note" in the footer), the model menu (Default,
  Opus, Sonnet, Haiku, Other…) and setup's "Or add: ~/Documents/GitHub" row.
- Look v004's host was stopped, its candidate released and its root removed.
  v005 builds the seven notes, recorded in the RFC, and the cut-off last line
  the first screenshot showed.

## Look v005 (2026-10-23)

- Frozen from `a59f982ce` (on main `055bed96e`, with look v004's notes:
  inline code coloured as the pull request's language, Go to note, models
  with their versions, the suggestions above the field with a show/hide
  toggle, file-and-line links, the prefilled folder line with no "Or add",
  the answer's last line whole, and the pull request in the header); the
  launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`. Root
  `/Users/jaeger/bpr0092-agent-setup-v005.i823o3r4`, with the stand-in clone
  under `clones/review-repository`.
- Session `3bf1ae97f1f725024c467518eb4519f2` (fresh, short; state
  `~/.attended-review-state/bpr0092-look-v005`), served with
  `serve --tailscale`; one question, the look:
  - Q1 (AC1, RFC AC14), the Agent tab after the v004 notes: "The first time
    I open the Agent tab it tells me what to do, and I end up asking a
    question that the agent answers from my repository."
- Handed back 2026-10-24: **Q1 Reject**. The owner's note, verbatim (line
  breaks joined): "This is good but I want the suggestions to draw more like
  they're in the chat window rather than attached to the bottom pane. That
  way they show up after the last chat message not overtop of it. The
  explain review.cpp line 36 option should change to explain selected block
  or something when there's a selection, especially a multi-line selection.
  The review link in the response isn't scrolling the window to the line
  when i cmd click it." Their screenshot showed the four suggestions stacked
  over the end of an answer above the field, and the comparison still at
  lines 110–116 after the answer's `review.cpp:36` link was pressed.
- Owner, in chat (relayed), verbatim: "Very close, i think the setup wizard
  is done. Just a little more to go." **The setup wizard is accepted**: its
  Agent, Repository, Check it works and Ready steps as look v005 showed them.
  What remains of Q1 is the conversation.
- Look v005's host was stopped, its candidate released and its root removed.
  v006 builds the three notes.

## Look v006 (2026-10-24)

- Frozen from `4d3105b51` (on main `055bed96e`, with look v005's notes: the
  suggestions as the thread's last item, "Explain the selected block" for a
  multi-line selection, and file-and-line links that scroll the code to their
  line); the launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`.
  Root `/Users/jaeger/bpr0092-agent-setup-v006.5unftz0z`, with the stand-in
  clone under `clones/review-repository`.
- Session `1cdefc512bbd4c5ceabdc9a54b1cffeb` (fresh, short; state
  `~/.attended-review-state/bpr0092-look-v006`), served with
  `serve --tailscale`; one question, the look at the conversation (the setup
  wizard is accepted):
  - Q1 (AC1, RFC AC14): "The first time I open the Agent tab it tells me
    what to do, and I end up asking a question that the agent answers from
    my repository."
- Handed back 2026-10-24: **Q1 Reject**. The owner's note, verbatim (line
  breaks joined): "Codex is totally missing its model settings. And its
  effort is missing max. This model picker dropdown should have a link to
  open the agent settings page. Updating the suggestion text seems either
  extremely slow or busted. I selected a block and it still said the iine
  suggestion and then when i clicked away to unselect the block thats when
  the suggestion text updates to block I dont think we should have this
  option to go back to the wizard, actually we can keep it if it's a "reset
  agents" which clears all settings and goes back to the wizard fresh."
  Their screenshots showed Codex's Model menu holding only Default and
  Other…, its Effort menu minimal to xhigh, and "Explain the selected block
  (review.cpp lines 29–35)" offered while one line, 42, was selected.
- Look v006's host was stopped, its candidate released and its root removed.
  v007 builds the five notes.
