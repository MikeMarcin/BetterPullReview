---
schema: "implement-rfc.card.v1"
id: "BPR-2146"
title: "Suggested prompt macros: storage and the Settings roster"
status: "Done"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2145"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC11-AC13"}
acceptance: [{"criterion":"AC11","outcome":"pass","revision":"cacec6738","evidence":{"kind":"command","detail":"application_prompt_macros_public_path: a chip turned off leaves the open window; a new order by Option-Up and by drag on Settings › Prompts reaches the chips; both survive a restart"}},{"criterion":"AC12","outcome":"pass","revision":"cacec6738","evidence":{"kind":"command","detail":"sqlite_agent_setup_storage (no tables reads the shipped set; a stale write changes nothing; a newer row is kept); application_prompt_macros_public_path (an unreadable store leaves the shipped chips working and Settings › Prompts says edits are unavailable)"}},{"criterion":"AC13","outcome":"pass","revision":"cacec6738","evidence":{"kind":"command","detail":"application_prompt_macros_public_path: the suggestions toggle hides every chip; Reset agents changes no prompt; Restore built-ins… returns the built-ins and keeps the reviewer's own"}}]
---

# Suggested prompt macros: storage and the Settings roster

Profile `agent-setup.sqlite3` tables holding only differences, revisions refusing stale writes, upgrade/downgrade/failure rules, and the Settings roster with On, drag and keyboard order, and Restore built-ins....

## Acceptance

RFC-BPR-0125 AC11-AC13.

## Progress (2026-11-14, built for RFC-BPR-0128 AC04)

Claimed because RFC-BPR-0128's AC04 (Quick ask saved with the macro) needs a
store and an editor. Built: the profile's differences in `agent-setup.sqlite3`
(`prompt_macros`, `prompt_macro_orders`, `prompt_macro_revision`; stable names,
so a newer build's row is read as newer, kept and never rewritten; a write
names its revision and an older one is refused), `application::prompt_macro_host`
(read at start and when the window comes to the front; the chips, the tour
chats, the Ask menus and Settings follow it), and Settings' Suggested prompts
roster (`qt::prompt_macros_panel`: a list a place, On, state in words, Return
or double click opens the sheet, Alt+Up/Down or a drag orders, Restore
built-ins…, an unreadable store's sentence).

Evidence: `sqlite_agent_setup_storage` (AC12: no tables reads the shipped set;
a stale write changes nothing; a newer row is kept), and
`application_ask_menu_public_path` (a built-in's change and an own macro
saved through Settings, read back from the store and reaching the menus).

Not yet evidenced, so the card stays In Progress: AC11's turning off a chip in
an open window and its restart (no course yet), AC12's unreadable store on
screen, AC13 (the suggestions toggle and Remove every agent, Restore
built-ins… through the window), and the drag's announcement.

## Progress (2026-11-14): AC11 and AC13 through the window

New course `application_prompt_macros_public_path` (the composed window, the
scoped stub as the agent): a chip turned off in Settings leaves the open
window without a restart; a new order made by keyboard (Alt+Up) reaches the
chips; both survive a restart of the application on the same profile (AC11);
the suggestions toggle hides every chip and shows them again as they were;
Reset agents and setting up again changes no prompt; Restore built-ins…,
after its question, turns the built-in back on in the shipped order and keeps
the reviewer's own prompt (AC13).

Still open: AC11's order made by drag (only the keyboard is exercised), its
drag announcement, and AC12's unreadable store on screen (the store's side is
in `sqlite_agent_setup_storage`). BPR-2179 is rebuilding this roster as a
Prompts section; these courses follow it.

## Progress (2026-11-14): AC11's drag and AC12 on screen; Done

- **Drag** (`cacec6738`): the course drags the last placement on Settings ›
  Prompts' pull request chips above the first. It sends enter, move and drop
  on the list's viewport, carrying the data the list itself puts in a drag.
  The surface takes it, the placement stands first, the chips follow in the
  open window, and the order survives the restart with the keyboard's.
- **AC12 on screen**: after the restart course, the store's prompt orders
  table is replaced with one this build can't read, and the application
  starts again on the same profile. The shipped chips work, and Settings ›
  Prompts says "Your prompts can't be read, so the built-ins are shown and
  changes aren't saved." (`promptsStatus`). With `BPR_ASK_RENDER_ROOT` set,
  the course saves a render of it (`prompts-unreadable-store`).
- Ran three times in a row, all passing. One earlier run of the course
  before these steps were added failed once after 48 seconds and passed on
  the next run; its output was not kept.
- Not in this card: the move's announcement ("Moved to 3 of 6") is RFC-BPR-0125's
  roster text, and its screen-reader acceptance is AC18 (milestone 4). The
  Prompts page that replaced the roster is [[BPR-2179]]'s; it has no
  announcement yet.

AC11 to AC13 are evidenced through the window; the card is Done.
