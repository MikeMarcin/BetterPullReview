---
schema: "implement-rfc.card.v1"
id: "BPR-2179"
title: "Prompts as a settings section of its own, where editing a quick ask is obvious"
status: "Parked"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2163"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2179--BPR-2179--371a8a5edb5b.json", "origin": "owner look ask-v002 (session 7268b4da), 2026-11-14, BPR-2163-quick-ask-editor rejected"}
parked_on: "human-review"
---

# BPR-2179: Prompts as a settings section of its own

The owner, rejecting `BPR-2163-quick-ask-editor` in look ask-v002
(2026-11-14):

> It's not really clear that you can open and edit these. I feel like we can do better on the UX design. I also think we should split agents and prompts into separate settings sections.

[BPR-2163](../60-done/BPR-2163-quick-asks-and-their-scopes.md) is Done (its
core, store and sheet stand; Done is terminal), so the rework is this card.
The coordinator asked for BPR-2163 to reopen; a Done card does not move, so
this follow-up carries the work instead.

## Scope

Load `frontend-design:frontend-design` first. Settings gets a Prompts section
beside Agents (Agents & context keeps the agents); the suggested prompts,
their Quick ask and scopes move there, laid out so that editing one is plainly
offered. Three mockups go to an owner look first; nothing is built until one
is chosen:

- A: the list and the editor side by side; picking a prompt shows its fields.
- B: the three Ask menus drawn as they show (pull request, file, thread), each
  row with an Edit button, drag to reorder, Add to each; the chip-only prompts
  below.
- C: one table with an In Ask menu switch and an Edit… button on every row,
  filtered by scope; Edit… opens a regrouped sheet.

Ask > Edit Quick Asks… opens the new section.

## Acceptance

- **AC01:** Settings has a Prompts section separate from the agents' section, holding every suggested prompt with its Quick ask and scopes, in the layout the owner chose.
- **AC02:** Each prompt shows a visible way to edit it without hover or a double-click, and Ask > Edit Quick Asks… opens the section.
- **AC03:** The owner accepts the built section in a look.

## Notes

Parked on the owner's choice of mockup (look ask-v003).

## Grill round, 2026-11-14 (owner)

The owner rejected mockups A, B and C (look ask-v005). Their note, verbatim: "I like parts of all of these. I like the filter of C. I like the larger edit area of A. I like the drag reordering of B. I like being able to see at a glance if they're in the quick ask or suggested chips. ... There's like a concept missing of conditions or triggers. ... I think we need a grill-rfc to align on what our requirements are."

Settled:
- **Model:** one prompt text with placeholders (`{file}`, `{lines}`, `{thread}`). It is ticked for any of the scopes (pull request, file, thread), and each ticked scope chooses its places: the Ask menu and/or the suggested chips.
- **Conditions:** chosen from a fixed, named set of built-in conditions, all of which must hold. Examples: new pushes since my review; replies since my comment; the file has unresolved threads; I haven't reviewed this file; the PR is mine or not mine; the thread is unresolved. Free-form rules are out of scope.
- **Chips:** the user's drag order sets the order, and the first 5 prompts that match are shown.
- **Built-in prompts:** fully editable, with "Reset to default" on each. They can be turned off but not deleted.
- **Storage:** two layers. An app-wide list, with per-repository overlays and overrides.
- **Layout ingredients the owner liked:** C's filter, A's large edit area, B's drag reordering, and seeing at a glance whether a prompt is a quick ask or a chip.

Open, and this blocks the build: how one prompt shows its several placements, both in the list and in the editor. For example, "Was this addressed?" is in the Ask menu and is a chip for a thread, and is in the Ask menu for a PR. The owner asked for mockups from a deep worker to choose between. It is still open how the repository overlay looks in the settings UI (which values are overridden, and how to revert to the app-wide value).
