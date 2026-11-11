---
schema: "implement-rfc.card.v1"
id: "BPR-2115"
title: "Name a tour chat session instead of showing its number"
status: "Backlog"
rfcs: ["RFC-BPR-0100"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2115--BPR-2115--595cd4a2b09f.json","origin":"owner feedback note on RFC-BPR-0100-chat-lower-edge, session c46887c9, 2026-11-11","related_cards":["BPR-1995"]}
depends_on: []
---

# Name a tour chat session instead of showing its number

## Origin

The owner, after accepting the chat's lower edge in session `c46887c9`
(2026-11-11), as a suggestion rather than a rejection
([RFC-BPR-0100 ledger](../../verification/RFC-BPR-0100.md)): "Instead of
session number can we get the name/title of the session from the agent?
Maybe that title wouldn't be good I dunno. Being able to name it like vscode
let's you would be cool."

## Today

The tour side chat's Session menu shows the session's ordinal ("Session 3",
[[BPR-1995]], `tour_side_chat.cpp`), counted from the stored sessions newest
first. History lists the same sessions.

## Scope

Show a session's name instead of its number, in the Session menu's label
and in History:

- a name the reviewer gives (rename, as VS Code allows), stored with the
  session and kept across restarts;
- otherwise, where the agent reports a session title or summary, that
  title; otherwise the number as today.

Open question for the owner before it is built: whether the agent's own
title is wanted at all ("Maybe that title wouldn't be good"), and whether a
name also applies to Agent tab sessions. Load `frontend-design:frontend-design`
before the Qt work.

## Intended verification

Core or host tests for the label's choice (given name, agent title, number);
storage round trip of a given name; a Qt test that renames from the menu;
an owner look on a frozen build.

Not started: Backlog. No requirement of RFC-BPR-0100 changes until an RFC
amendment is written for it.
