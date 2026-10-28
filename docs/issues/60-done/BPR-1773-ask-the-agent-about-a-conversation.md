---
schema: "implement-rfc.card.v1"
id: "BPR-1773"
title: "Ask the agent about a conversation: review threads and private notes, with the code they are on"
status: "Done"
rfcs: ["RFC-BPR-0094"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1772--BPR-1773--b27f9d2285df.json","kind":"feature","found_by":"owner chat, 2026-10-26","authoring":"frontend-design:frontend-design loaded for the Qt surfaces","related_cards":["BPR-1594","BPR-1753","BPR-1245"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "d7322f02675655a3cd04dc58cf7a52c6b12498b6", "evidence": {"kind": "command", "detail": "core_agent_chat_scope (eighth entry point, no selection needed) and core_agent_context_supplement (test_a_conversation_travels_bounded_or_refuses_the_request: bounds, opening and newest comments, excerpt window, refusals, carried unchanged or refused whole)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "d7322f02675655a3cd04dc58cf7a52c6b12498b6", "evidence": {"kind": "command", "detail": "application_agent_launch, test_a_conversation_is_read_and_crosses: read_conversation (current and original lines, deleted comments left out) and every conversation.* field after the version-two fields; none without one."}}, {"criterion": "AC3", "outcome": "pass", "revision": "7dacd2e9dd478883297a7fb386b022e50182b5b7", "evidence": {"kind": "command", "detail": "r2 (owner look 2026-10-27): qt_workbench_window_scale_100 (the asked line reads \"About <conversation>: <question>\" in the question's colour and a heavier weight; every answer's footer has Draft comment beside Save as note in its style, on a conversation turn and an ordinary one; Draft comment becomes Go to draft and Go to draft drafts nothing more; a refusal says why; no Use as reply; Save as note and Go to note unchanged), qt_conversation_cards_view, qt_private_notes_panel."}}, {"criterion": "AC4", "outcome": "pass", "revision": "7dacd2e9dd478883297a7fb386b022e50182b5b7", "evidence": {"kind": "command", "detail": "r2: application_agent_chat_conversation_contract: Ask agent from a fixture thread's menu through the shipped composition, then a send: the turn is ask_about_conversation, carries that thread, runs once; its asked line names the thread; its Draft comment writes the answer into that thread's reply (review editor shows a reply draft holding the answer), becomes Go to draft, and no submission or target observation follows; one launch path. application_private_review_host note_press unchanged. core_review_submission: answer_draft_place_for."}}, {"criterion": "AC5", "outcome": "pass", "revision": "9ea99c184ceab216f1cf9d966b50549dc012c2a8", "evidence": {"kind": "observation", "detail": "Owner's attended looks, 2026-10-27: r2 question line accepted (session e2c47ec080c11512d672f942f5930e5c); r3 footer, with the copy glyph repainted for dark mode, accepted (session a97a57a4798edac2287083d44cc0a1b3, candidate BPR-1773-footer-r3)."}}]
---

# BPR-1773: Ask the agent about a conversation

Implements [RFC-BPR-0094](../../rfcs/RFC-BPR-0094-ask-the-agent-about-a-conversation.md),
whose problem, owner decisions and criteria this card does not repeat.

## Slices

- Core: the entry point and the bounded conversation (`686c8c448`).
- Application, composition and Qt: threads, notes, the chip, Use as reply.
- r2 rework after the owner's look (`7dacd2e9d`): Draft comment on every answer
  replaces Use as reply; the asked line names the conversation.

## Evidence

AC1 and AC2 pass at the recorded revision; AC3 and AC4 pass again at the r2
revision (front matter). Thirteen focused
ctest entries pass on macOS dev, including the composition courses
`application_agent_chat_composition_contract` (unchanged) and
`application_agent_invocation_composition_contract`.

## Not run

- The full suite, `coding-tidy`, Windows and Linux.

## Owner's look, 2026-10-27: rejected

> Seems really weird that's there's already a save as note and we added a
> second one. The ux doesn't make sense to me I don't even understand what the
> question I asked was and what the agents response is. And we may as well
> just let you turn anything into a note or draft comment directly if we're
> doing it for this. The preexisting save as note goto note looks much better
> than this.

Rework:
- A turn about a conversation shows plainly what was asked and about which
  conversation (the chip's text on the asked line), then the answer.
- Retire the separate "Use as reply" button. Every agent answer's footer
  offers the existing actions in the existing style, beside **Save as note**
  and **Go to note**: **Draft comment**, which on a conversation turn becomes
  a draft reply to that thread.

### Rework done (r2, 2026-10-27)

- The asked line: a turn about a conversation names it above the question,
  in the question's own colour and a heavier weight (r1 had a grey "About"
  line); the bubble's accessible name is "About <conversation>: <question>".
- Use as reply is gone. Every answer's footer reads Copy, Save as note,
  Draft comment, in one quiet style. Core's `answer_draft_place_for` places
  the draft when the question is sent: the thread's reply (a thread the
  viewer may reply to), else a line comment on the selected lines (the
  finding card's draft path), else Overview's new pull request comment.
  Drafted, it reads Go to draft. A restored turn drafts on the pull request.
- Not claimed: a note conversation drafts by lines or the pull request, not
  on the note's line; the place is not kept with a restored turn.
- Found while composing the course: the fixture thread
  `PRRT_fixture_2` is routed by the window as a reply on the thread
  (on_thread), but the host's `thread_reply_binding_of` refuses it, so Reply
  there silently does nothing. Pre-existing and outside this card; the
  course asks about a thread on a current line instead.

## Owner's look at r2, 2026-10-27

Session `e2c47ec080c11512d672f942f5930e5c`, two candidates at `e4c7ac308`:

- BPR-1773-question-line (the asked line names its conversation): accepted.
- BPR-1773-footer (Draft comment beside Save as note): rejected, only for
  the copy glyph:

> The copy to clipboard icon is nearly unseeable on dark mode but otherwise
> good.

### Rework done (r3, 2026-10-27)

- The agent tab painted its glyphs once, in the palette's ink when each was
  made, so a theme change left Copy, and the composer's plus, close and send
  glyphs, in the light theme's dark ink. A palette change now paints each
  again (`agent_tab_view::repaint_glyphs`).
- `qt_workbench_window_scale_100` (test_the_agent_is_a_drawer_tab): after a
  switch to dark, Copy is drawn in light ink.

## Owner's look at r3, 2026-10-27: accepted

Session `a97a57a4798edac2287083d44cc0a1b3`, candidate BPR-1773-footer-r3:
accepted. Every criterion passes.
