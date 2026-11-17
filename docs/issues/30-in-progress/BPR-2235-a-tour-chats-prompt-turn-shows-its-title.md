---
schema: "implement-rfc.card.v1"
id: "BPR-2235"
title: "A tour chat's prompt turn shows the prompt's title"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# A tour chat's prompt turn shows the prompt's title

## Origin

In the Agent tab a turn sent from a prompt shows the prompt's name, with Show prompt for what was sent ([[BPR-2219]], AC-h; owner, 2026-11-16: "All of this extra context from a prompt we select should be hidden.. it's just noise"). The tour chats (a stop, Problem, Solution, the whole tour) still show a named prompt's whole text.

## Scope and acceptance

- **AC-a.** A tour chat's turn sent from a prompt shows the prompt's name as the user's turn.
- **AC-b.** What was sent is behind the same small disclosure the Agent tab uses.
- **AC-c.** A typed message shows as typed.

## Delivered

A tour chat (a stop, Problem, Solution, the whole tour) sends a prompt
pressed as offered with its name (`tour_ask::Name`, only when the name is
not the text). The host remembers it in the profile's prompt turn names, as
the Agent tab's turns are, and the chat reads names back through
`set_turn_name_source`. The turn's bubble shows the agent glyph and the
prompt's name with Show prompt on one row; Show prompt shows what was sent,
Hide prompt hides it. A prompt put in the composer and sent, or anything
typed, is sent with no name and shows as typed.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | `qt_tour_side_chat`: a whole-tour turn named "Shape check" shows the glyph and the name, the question hidden | Pass |
| AC-b | The same: Show prompt shows "Too many stops?", Hide prompt hides it | Pass |
| AC-c | The same: "Lower, please." typed shows as typed, with no name; a prompt whose text is its name is sent with none | Pass |

Shown to the owner in the batched look.
