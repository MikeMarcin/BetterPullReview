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

## Checkpoint, 2026-11-16

Audit of the 15 facts against the window: `file_open`, `selected_lines`,
`path`, `takes_replies`, `empty_chat`, `pushes_since_review`, `draft`,
`author_is_me` and `repository` were already filled; thread pushes
(`thread_activity`'s pushes) too. Unfilled were `thread_unresolved`,
`file_unresolved_threads` (a thread whose resolution was never handed in
counted as resolved, so "has unresolved threads" never held),
`thread_activity`'s replies, `replies_since_mine`,
`file_unreviewed` and `checks_failing`.

Wired: `core::decide_thread_replies` (authors and the viewer's login) and
`core::decide_checks_failing` (each check's end, the head, whether checks and
statuses were both read) in `quick_ask.hpp`; `quick_ask_facts::ThreadsRead`,
and `file_unresolved_threads` unknown while a thread's state is. The window
hands in each thread's resolution as `core::project_thread_resolution`
projects the read's over the viewer's own change, the comment authors and
`snapshot.Viewer.Login`, and the files the Overview counts done
(`application::overview_done_files`) once it has read every receipt; the
suggestion context hands in the checks. Every surface (the three Ask menus,
the Agent tab's and tour chats' chips, a queued press) reads these one set of
facts.

## Verification, 2026-11-16

On the lane's build/dev (with the other workers' uncommitted files present):
`core_quick_ask` (new `test_window_facts`: replies and replies since mine from
authors and the viewer, case-insensitive logins, deleted comments, no viewer;
checks per end, head only, unread statuses unknown; thread unresolved, file
unresolved threads incl. unknown and threads unread, file unreviewed, and the
file menu hiding a prompt once its file is marked reviewed),
`core_prompt_macro`, `core_prompt_surfaces` pass.
`application_prompt_facts_public_path` (new, returning-review-v1, viewer
jiayin) passes: own prompts with Hide while unmet on the pull request's, a
file's and a thread's Ask menus are listed exactly where author_is_me, draft,
checks_failing, pushes_since_review, repository, path,
file_unresolved_threads, file_unreviewed, thread_unresolved, thread_activity
(replies), replies_since_mine and takes_replies hold (tbutler replied after
jiayin on PRRT_cc_1/cc_4, jiayin last on cc_3, never on cc_7); Resolve on
PRRT_cc_7 through its card flips thread_unresolved there and
file_unresolved_threads on slider.cpp; marking renderer.cpp read flips
file_unreviewed. `application_ask_menu`, `_ask_addressed`, `_prompt_macros`
and `_ask_queue_public_path` still pass. Format and documentation gates pass.

Not covered by the course: `selected_lines`, `file_open`'s "is not" and
`empty_chat` are chip-only or already wired and untouched; a reply arriving
from another account mid-session is shown by the fixture's static authorship,
since a re-read of the fixture serves the same threads. Nothing visible
changes beyond prompts appearing, so no look is filed.
