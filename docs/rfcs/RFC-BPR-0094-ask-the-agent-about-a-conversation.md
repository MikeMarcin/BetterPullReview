---
id: "RFC-BPR-0094"
title: "Ask the agent about a conversation"
status: "In Progress"
area: "BPR"
depends_on: ["RFC-BPR-0012", "RFC-BPR-0023", "RFC-BPR-0092"]
metadata: {"origin": {"date": "2026-10-26", "channel": "owner chat"}, "card": "BPR-1773", "revision": "r2 2026-10-27, owner look rejected: Draft comment on every answer replaces Use as reply", "authoring": "implement-rfc; frontend-design:frontend-design loaded for the Qt surfaces"}
---

# Ask the agent about a conversation

In Progress, revision r2. Card [[BPR-1773]].

## Problem

The owner, 2026-10-26, verbatim:

> I want to be able to ask the agent about a conversation in comments, and
> probably notes too. Can we design and build a low friction way to get an
> agent thread with context from a conversation including the code its
> attached to?

Observed before this RFC (`ad39b7ce1`):

- The Agent tab ([[BPR-1594]], [[BPR-1753]]) keeps one conversation per
  pull request and agent. Every review request already carries every review
  thread in its brief (RFC-BPR-0092), but nothing lets the reviewer say
  *which* conversation a question is about.
- **Ask about these lines** (Ctrl+I) needs a code selection: core refuses
  `ask_about_selection` without one (`selection_required`), so a question
  from a thread in Discuss, or with no file open, cannot be asked.
- No thread card or note card offers anything agent-related. The side chat's
  context chips are never filled.
- A private note cannot be sent: RFC-BPR-0023 refuses any run carrying a
  mutable note; only immutable Private history revisions may be selected.

## Owner decisions (2026-10-26, in chat)

- **Q1, where the thread lives:** in the pull request's own Agent thread,
  with the conversation attached to the next question as a removable chip
  (not a separate thread per conversation).
- **Q2, Use as reply:** yes. An answer about a review thread offers **Use as
  reply**, which writes it in that thread's reply field as a local draft.
- **Q2, amended by the owner's look of 2026-10-27 (rejected):** "Seems
  really weird that's there's already a save as note and we added a second
  one. The ux doesn't make sense to me I don't even understand what the
  question I asked was and what the agents response is. And we may as well
  just let you turn anything into a note or draft comment directly if we're
  doing it for this. The preexisting save as note goto note looks much better
  than this." Use as reply is retired. Every answer's footer offers **Draft
  comment** beside **Save as note**, in its style, and it becomes **Go to
  draft** as Save as note becomes Go to note. On a turn about a review thread
  it is that thread's reply.
- **Q3, notes:** included now. Ask agent on a note saves the note as an
  immutable Private history revision first and sends that copy, as the
  comment workshop does with drafts ([[BPR-1245]]).

## Behavior

- **Ask agent** is on an open review thread's header (the Agent tab's own
  mark) and in its menu ("Ask agent about this conversation"), and on a
  saved private note's card. Pressing it opens the Agent tab with the keyboard
  in the question field and a chip naming the conversation ("src/cache.cpp
  lines 41–42, 2 comments", or "Your note on src/cache.cpp line 42"). Nothing
  is sent.
- While the chip is there the field says "Ask about this conversation…", the
  suggested prompts are about it (Summarize this conversation; Is the concern
  raised here right?; Draft a reply to this conversation, or for a note,
  Explain the code this note is on), and each turn asked with it names the
  conversation on its asked line, in the question's own colour and a heavier
  weight, above the question: the bubble reads "About src/cache.cpp lines
  41–42, 2 comments" then "Who is right?", and its accessible name is "About
  …: …". The chip stays for follow-ups; its close button
  removes it. Opening another pull request removes it.
- A turn asked with the chip is the eighth entry point,
  `ask_about_conversation`. It needs no selection (a selection, when there is
  one, still travels as context and anchors findings). It carries the brief
  as every review request does, and `conversation.*`: the subject,
  path, lines, state, comments (bounded), and up to 40 lines around the anchor
  as the compared file shows them. See the
  [AI agent boundary](../architecture/ai-agent-boundary.md#context-supplement).
- **Draft comment** (r2) is in every answer's footer, after Copy and Save as
  note. Where it drafts is decided when the question is sent, by
  `core::answer_draft_place_for`: the review thread the question was about
  (one the viewer may reply to), else the lines selected beside the code,
  else the pull request. It writes the answer (or the selected part):
  - in the thread's reply, opened in place as Reply does, after any words
    already there; if the reply cannot open within five seconds the words go
    to the clipboard and the action says why;
  - as a line comment on the first selected line, by the finding card's own
    Add as draft comment path (the file opens first when another is shown);
  - in Overview's new pull request comment ([[BPR-1781]]), written as if
    typed, so the composer keeps it as its private draft.
  It then reads **Go to draft**, which opens that reply, line or composer. A
  refusal reads "Not drafted: …" and the action can be pressed again. A turn
  restored from an earlier session, or a review request, has no recorded
  place and is drafted on the pull request. Nothing is published until the
  reviewer publishes.
- **Notes.** Sending a question with a note's chip first records the note's
  text as one reviewer revision on a new pull-request-level private thread
  (a file-level one would need the note's comparison to be the current one),
  then sends the turn as a private run whose only source is that revision; the
  note's file and line travel in `conversation.*`. If it cannot be recorded,
  nothing is asked.

## Architectural allocation

- **Core:** `agent_entry_point::ask_about_conversation`;
  `conversation_input`, `supplement_conversation`, `bound_conversation` and
  `valid_conversation`; the supplement carries the conversation and refuses a
  malformed one whole.
- **Application:** `read_conversation` (one review thread, deleted comments
  left out, outdated threads at their original lines); the observation's
  `Conversation`; the `conversation.*` encoding;
  `private_review_application_host::record_note_press`.
- **Composition:** which conversation is attached; its excerpt from the
  compared file; the entry point on send; the note's Private history copy
  before the turn.
- **Core (r2):** `answer_draft_place` and `answer_draft_place_for`.
- **Composition (r2):** the place each sent question's answer would be
  drafted, kept by an opaque key the turn holds; the draft through the
  existing reply, finding-card line draft and new-comment paths; Go to draft.
- **Qt:** the card and note actions, the chip, prompts, the asked line, and
  Draft comment / Go to draft (the window's `draft_thread_reply`,
  `draft_pull_request_comment` and their `open_` counterparts). Widgets
  perform no effect: every gesture is a callback the composition decides.

## Acceptance criteria

- AC1. Core: the entry point needs no selection; the conversation bound keeps
  the opening and newest comments, cuts bodies and the excerpt, refuses
  malformed input and a note with comments, and the supplement carries it
  unchanged or refuses it whole.
- AC2. Application: a thread is read as specified and its fields cross after
  every earlier field; a launch without one writes none.
- AC3. Qt: Ask agent on a thread (header and menu) names it to the host; the
  chip, prompts, the asked line and Draft comment / Go to draft behave as
  specified, and no answer has Use as reply.
- AC4. Composition: a turn with a thread attached is `ask_about_conversation`
  and carries that thread, and its Draft comment writes the answer into that
  thread's reply and publishes nothing; with a note attached, the note is recorded in
  Private history first and the run's only private source is that revision.
- AC5. Owner look on a frozen build: the thread's Ask agent, the chip, a turn
  about a conversation (its asked line) and the answers' Draft comment / Go
  to draft beside Save as note (attended review, one question each). r1's
  look was rejected on 2026-10-27; r2 awaits a fresh look.

## Not claimed

The excerpt only when the thread's file is the one compared (the agent's
workspace holds the file in every case); pull-request-level comments and
review bodies (no Ask agent yet); REST-only anonymous reads; the asked line's
conversation and Draft comment's place after the thread is reloaded (neither
is kept with the turn; a restored turn drafts on the pull request); Windows and Linux
native runs.
