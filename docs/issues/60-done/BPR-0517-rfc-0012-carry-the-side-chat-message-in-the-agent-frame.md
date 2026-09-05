---
id: BPR-0517
title: "Host: carry the side-chat message in the agent's input frame"
status: Done
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

## What this slice delivered — 2026-09-02

The message rides the supplement, at one position, and the agent counts it.
`core::agent_context_supplement` gained a bounded `Message` and
`core::agent_chat_message` — a session identity and the words, and nothing a
note, a draft, or a selection could occupy. `build_agent_supplement` admits it
under the rule `admit_private_context` already used: a message naming another
session, or one past `maximum_chat_message_bytes`, refuses the whole
supplement. `encode_agent_supplement` writes it once, as `chat_message`
immediately after `pull_request_url`; that is the supplement's whole wire form,
so `frame_agent_input` carries it into every protocol's frame exactly once —
after the `BPR-CONTEXT/2` header for Codex JSONL and the external terminal,
inside the escaped user text for Claude stream JSON, and on App Server's
deferred context, whose first frame is a handshake with no context in it.
Nothing is appended beside the payload, which is what keeps "exactly once"
true on three protocols and true at all on the fourth, and the decode side is
untouched. The chat host's send intent is the only thing that fills the field:
`dispatch_agent_invocation` takes the message as an argument, the chat
dispatcher passes `aTurn.Message`, and the launch panel's start passes empty.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The core admits a typed message only for its own session, and only within its bound | `ctest --preset dev -R '^core_agent_context_supplement$'` | Passed 0.16s. A supplement built outside a chat turn carries no message; one built with it carries the exact bytes and changes nothing else; an out-of-session message refuses the whole build under both overloads; `maximum_chat_message_bytes` is admitted and the byte past it is refused whole; a message with no opt-in still carries zero notes and zero drafts | this commit |
| The typed message appears in the canary's received frame exactly once | `ctest --preset dev -R '^application_agent_chat_composition_contract$'` | Passed 14.00s. The fixture agent counts the message in its own stdin and echoes `BPR_CHAT_ECHO_0517 count=1 text=...`, asserted on the queued claim, beside `LastTurn->Message` being what was typed | this commit |
| Deleting the field's placement fails that contract | delete `append_field(result, "chat_message", aSupplement.Message);` (`src/application/src/agent_launch.cpp:1560`), rebuild, rerun | Failed as required: the agent reported `count=0 text=` — it received no question at all — while every other conjunct stayed 1 and the pass marker was absent. Restored byte-exact, md5 `1c672b7705bc94de798ece4f6add58e9` before and after | this commit |
| Nothing but the chat's send intent fills the field | `ctest --preset dev -R '^application_agent_finding_composition_contract$'` | Passed 13.79s. The launch panel's start gesture is not a chat turn and its frame carries `BPR_CHAT_ECHO_0517 count=0 text=` | this commit |
| `continue_against_old_revision` is driven from a stale projection to one new turn against the old revision | `ctest --preset dev -R '^application_agent_chat_continued_contract$'` | Passed 22.93s. Choosing it raises continuations by one and starts and adopts no session; the chat keeps its identity and its old binding, asserted unequal to the revision under review; `Required` clears while `head_moved` stays; no process starts. One further send then dispatches exactly one turn under that kept session — old id, old binding, the second message's exact bytes — with a second process, a second validated delivery, and 0/0/0 GitHub mutation | this commit |
| The sibling RFC-0012 selectors stay green | `ctest --preset dev -R '^(core_agent_context_supplement\|core_agent_chat_scope\|application_agent_launch\|application_agent_finding_protocol\|application_agent_chat_[a-z_]*contract\|application_agent_finding_composition_contract\|composed_application_hosts\|application_review_round_trip_composition_contract)$' --output-on-failure` | 11/11 passed in 112.33s | this commit |
| The two app-source digest pins are repinned | two passes over `application_review_round_trip_composition_contract` | `bpr_expected_full_application_digest` → `408d9746baca5ff19e98feaabdc4343d50e3e87dfd94669dfa939484063babe1`; `bpr_expected_production_app_manifest_digest` → `ff2c96c346a99903e7738e5b486e1a1e1dea1818baf9a1c8a13cd188c2abeaff`. The second drift was hidden until the first was repinned, and both were taken again after the formatter touched `github_application.cpp` | this commit |

### What the continuation course had to be told, and why

The `stale` course moves the head by replaying an accepted handoff without its
smart-Git transport, which is the cheapest way to move a head and leaves the
new revision unprepared — fine for a course that only needs staleness, useless
for one that must go on to ask a question. The `continued` course therefore
moves the head with the shipped refresh gesture against the seeded recording,
and re-selects the file afterwards: the head moving takes the workbench
comparison with it, and `ask_about_selection` is one of the three entry points
the core refuses without a selection. Navigating to make one adds no context by
itself, which is the property BPR-0499 already proves.

One thing the course does not claim, because it is not true yet: the turn is
against the old revision in the session sense — the chat's own identity and
binding — while the context the launch path composes is still built from the
revision now under review, and the launch path then admits a session bound to
that revision. Continuing against an old revision does not yet re-freeze the
old comparison. That is a separate concern from framing a message and is not
this card's surface.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran the supplement, chat-scope, launch,
and protocol suites, the four chat contracts, the seven finding contracts,
`composed_application_hosts`, the round-trip contract,
`application_composition_contract` (111.65 s), and
`qt_agent_launch_canary`: 19/19. `clang-format` clean by exit code on the
eleven touched C++ files. The placement's home in `encode_agent_supplement`
rather than literally inside `frame_agent_input` is accepted with the reason
both doc comments give; the boundary document now states the field and its
position. The two follow-ups the slice named are on [[BPR-0506]].

## Follow-up: the invocation contract's field list — 2026-09-02

`application_agent_invocation_composition_contract` (RFC-BPR-0011) counts
the supplement's fields against an expected list, and this slice added
`chat_message` to the encoder without adding it there; the contract read
162 fields against 161 and was red at HEAD. The integrator's reproduction
had not included that selector. The list now carries `chat_message`, empty
for a launch-panel invocation, and the contract passes (30.41 s). Found by
the [[BPR-0506]] pass.
