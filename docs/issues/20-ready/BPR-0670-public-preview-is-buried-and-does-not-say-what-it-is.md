---
schema: "implement-rfc.card.v1"
id: "BPR-0670"
title: "Public preview is buried and does not say what it is"
status: "Ready"
rfcs: ["RFC-BPR-0028","RFC-BPR-0017"]
depends_on: []
priority: "P1"
---


# Public preview is buried and does not say what it is

The owner, 2026-09-07: "The public preview is buried deep in menus and looks
innocuous, I could barely find it. I'm not even sure if this is meant to be a
'continue without an account' option or a 'staged testing data' option."

It is the first: reading a public pull request signed out, over the
credential-free authority RFC-BPR-0017 built. Today it is a small button on the
session panel, which lives on Settings > Accounts and hosts, next to Connect,
and its name says nothing about signing in or not signing in.

A reviewer with no account has exactly one way to see anything, and it is
behind two navigations and a word that reads like a build channel.

## Scope

- The signed-out path is offered where a signed-out reviewer lands: on Home,
  beside signing in, in words that say what it does and what it costs ("read a
  public pull request without signing in; comments and review actions stay
  read-only").
- The name is decided with the owner. "Public preview" is not it.
- What the mode cannot do stays explained where the reviewer meets the refusal,
  as it already is.
- Owned paths: `src/adapters/qt` Home page and session panel, the mode's
  strings, their tests.

## Acceptance

- A signed-out reviewer reaches a public pull request from Home without opening
  Settings, and the control says what it does before it is pressed.
- The owner accepts the name and the wording.
