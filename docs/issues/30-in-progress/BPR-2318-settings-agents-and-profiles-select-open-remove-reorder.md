---
schema: "implement-rfc.card.v1"
id: "BPR-2318"
title: "Settings agents and profiles: a click selects, rows have grips and a trash, arguments are rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect-ux","reservation":"docs/issues/_reservations/BPR-2318--BPR-2318--0f4595873d94.json","origin":"owner chat 2026-11-20 (\"Agents settings: a single click on an agent opens it's settings page but that should just select it. Double click and enter to open. There should be a visible trashcan on each row to remove the agent with a confirmation. Aagents should have drag handles to reorder how they appear. Arguments in the agent settings isn't good, needs to show more than one row, needs drag handles, needs scroll bar. Ps profiles needs the same drag handle.\")","related_cards":["BPR-1753","BPR-2051","BPR-0778","BPR-2198"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2318: Settings agents and profiles: a click selects, rows have grips and a trash, arguments are rows

## The owner's words (2026-11-20, verbatim)

"Agents settings: a single click on an agent opens it's settings page but
that should just select it. Double click and enter to open. There should be a
visible trashcan on each row to remove the agent with a confirmation. Aagents
should have drag handles to reorder how they appear. Arguments in the agent
settings isn't good, needs to show more than one row, needs drag handles,
needs scroll bar. Ps profiles needs the same drag handle."

## What was wrong

- Settings › Agents ([[BPR-2051]]'s roster) opened an agent's sheet on a
  single click, so a row could not be selected without opening it.
- Remove lived only in the row's menu and removed at once, without asking.
- The roster could be dragged, but nothing on a row said so.
- The agent sheet's Arguments was a plain-text box capped at 72 pixels: about
  one line showed, with no grips and no visible scroll bar.
- Settings › Profiles listed profiles in identity order and could not be
  reordered at all; the registry had no order to save.

## Decisions

- Every agent can be removed, Claude Code and Codex included: Add agent…
  offers a popular agent again while it is found, so no row needs a disabled
  trash.
- Removing an argument does not ask: the sheet has its own Cancel.
- The agent order already lived in the core (`move_agent_entry`), was saved
  as the order of the agents list and is the order the footer's picker and
  the Agent tab use; it is unchanged. Only the presentation is new.
- The profile order is a registry fact, so it is saved with the registry. A
  registry is written as version 3 (a new `profile_registry_order` table)
  only when profiles are first reordered: one never reordered stays version 2,
  which builds before this card still read.

## Acceptance

- AC1. In Settings › Agents a single click selects an agent and opens
  nothing; a double-click, Return or Enter opens its sheet.
- AC2. Every agent row shows a trash at its right, named "Remove <agent>".
  It, Remove in the row's menu and Delete ask first ("Remove <agent>?",
  Cancel and Remove agent); only Remove agent removes it.
- AC3. Every agent row shows a grip at its left, named "Reorder <agent>". A
  drag moves the agent; Alt+Up and Alt+Down move the selected one. The order
  is the core's saved order, used by the footer picker and Agent tab.
- AC4. The agent sheet's Arguments shows one argument per row, five rows and
  a scroll bar past them, with a grip and a trash per row, Add argument,
  Alt+Up and Alt+Down, and Delete. Arguments are kept trimmed, in order, and
  an empty row is none.
- AC5. Every Settings › Profiles row has a grip, named "Reorder <profile>";
  a drag or Alt+Up and Alt+Down ask the host to move the profile, the list,
  the window's Profiles menu and the desktop menu bar follow the saved order,
  and nothing moves while a profile is being named.
- AC6. A registry saved before this card loads with no order and lists
  profiles by identity; the order survives a restart; a damaged order is
  refused.
