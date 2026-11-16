---
schema: "implement-rfc.card.v1"
id: "BPR-2221"
title: "Every prompt condition reads the window's real state"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2179"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2221--BPR-2221--0b9e7f5091ed.json","origin":"owner request, 2026-11-16: wire the prompt-condition facts that are never filled"}
---

# Every prompt condition reads the window's real state

## Origin

The owner asked, 2026-11-16, for the prompt-condition facts that are never
filled to be wired. The window's Ask menus, the Agent tab's chips and the
tour chats' chips are all decided from one set of facts
(`quick_ask_facts` in `window_quick_ask.cpp`). It never set a thread's
resolved state, whether it has replies, whether someone replied after my last
comment, or the files I have marked reviewed, and the suggestion context never
set whether a check failed. An unset fact is unknown, which satisfies neither
"is" nor "is not", so a prompt conditioned on thread is unresolved, file has
unresolved threads, new replies since the comment, replies since my comment,
I haven't reviewed this file or a check failed never showed, whatever the
pull request held.

## Scope and acceptance

- **AC-a.** Each of the 15 condition facts (`prompt_fact`) reflects the
  window's real state on every surface where it may be written (the Ask menus
  of the pull request, a file and a thread; the Agent tab's chips; the tour
  chats' chips), or is unknown only where the window really does not hold it.
- **AC-b.** "Me" is the signed-in account the read names: a thread has
  replies since mine when a comment by someone else follows my last comment on
  it; a thread I never commented in has none.
- **AC-c.** A file is reviewed when the review's own mark says so (read or
  accepted, as the Overview counts it done); a thread's resolved state is the
  accepted read's, as the viewer's own resolve or reopen projects it.
- **AC-d.** The facts follow that state: the menu is built fresh from it, so
  resolving a thread, marking a file reviewed or a reply by another account
  changes what a conditioned prompt shows.
- **AC-e.** The per-thread and per-check values are decided in core from raw
  data the window hands it (comment authors and the viewer's login; check
  conclusions); no policy in Qt.

## Intended verification

Core unit tests per fact (`core_quick_ask`). A public-path course on the
fixture GitHub server (returning-review-v1) where an own prompt conditioned on
each fact is listed or not in the Ask menu, and changes when a thread is
resolved and a file is marked reviewed. No look unless something visible
changes beyond prompts appearing.
