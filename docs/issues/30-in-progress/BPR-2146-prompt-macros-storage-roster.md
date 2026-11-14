---
schema: "implement-rfc.card.v1"
id: "BPR-2146"
title: "Suggested prompt macros: storage and the Settings roster"
status: "In Progress"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2145"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC11-AC13"}
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
