---
id: BPR-0517
title: "Host: carry the side-chat message in the agent's input frame"
status: Ready
rfc: RFC-BPR-0012
depends_on: [BPR-0500, BPR-0514]
---

# Host: carry the side-chat message in the agent's input frame

## Why

[[BPR-0514]] composed the side-chat session host: a send on the panel now
reaches one agent turn through the one dispatch site, a stale send without a
continuation is refused and presented, and a continuation starts exactly one
session. What the turn carries is the session, its scope, and its opt-ins.
The reviewer's typed message rides the turn as a value and stops there: it
never reaches the protocol frame, because the frame is built by
`frame_agent_input` from `core::agent_context_supplement`, which has no field
for a message. That framing is [[BPR-0500]]'s and was that card's stated
non-goal, so BPR-0514 named it as not delivered rather than widening into
another card's surface. Until it lands, a send is a session start with a
scope, not a question the agent can read.

## Required final state

- `core::agent_context_supplement` gains a bounded message field the core
  admits under the same session-keyed rules as the rest of the supplement:
  empty outside a chat turn, present only when the reviewer typed it, and
  never carrying private notes or drafts the opt-in did not admit.
- `frame_agent_input` places the message in the frame in one documented
  position, under the existing bounds; the protocol loop's decode side is
  unchanged.
- The chat session host fills the field from the panel's send intent and
  nothing else fills it.
- `continue_against_old_revision` is driven end to end once, since
  BPR-0514 implemented it without a course.

## Non-goals

- Any reply path change — replies already arrive through BPR-0500's finding
  delivery bounds.
- New panel fields — [[BPR-0502]].

## Required direct evidence

- `core_agent_context_supplement` (or the suite that owns the supplement)
  gains cases for the field's admission and bounds; an oversized or
  out-of-session message is refused by name.
- `application_agent_chat_composition_contract` asserts the typed message
  appears in the canary's received frame exactly once; deleting the field's
  placement fails it by name.
- A course drives `continue_against_old_revision` from a stale projection
  to one new turn against the old revision.
- The sibling RFC-0012 selectors stay green, and the two app-source digest
  pins are repinned if `github_application.cpp` changes.
