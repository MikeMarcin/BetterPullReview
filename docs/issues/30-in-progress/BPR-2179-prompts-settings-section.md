---
schema: "implement-rfc.card.v1"
id: "BPR-2179"
title: "Prompts as a settings section of its own, where editing a quick ask is obvious"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2163"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2179--BPR-2179--371a8a5edb5b.json", "origin": "owner look ask-v002 (session 7268b4da), 2026-11-14, BPR-2163-quick-ask-editor rejected"}
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

## Look prompts-settings-v002, 2026-11-14 (owner)

The owner chose none of directions 1 Grid, 2 Sentences and 3 Surfaces. Their note, verbatim: "Okay I think we need to split it into 2 separate elements.\n\nFirst we have surfaces which defines where prompts appear.\nThen we have the prompt library where we can create new prompts, edit existing prompts, and we drag/right click from the prompt library to add to surfaces."

Decided: the section has two parts.
- **Surfaces** define where prompts appear: the PR, file and thread Ask menus and their suggested chips.
- **The prompt library** is where prompts are created and edited.

A prompt is placed on a surface by dragging it from the library, or through a right-click menu. Next comes a round of mockups for that split.

Owner, 2026-11-14 (after v002), verbatim: "I think conditions are on the prompt itself, we can always duplicate a prompt if we need different conditions. Conditions can also be "any of these are true" or "all of these are true" Weak Auras editor (The wow Addon) had a good conditions UX."

Decided:
- A prompt's conditions belong to the prompt itself, not to where it is placed. To use different conditions, duplicate the prompt.
- Conditions combine as either "all of these are true" or "any of these are true".
- The WeakAuras trigger editor is the interaction reference.

## Look prompts-settings-v003, 2026-11-14 (owner)

The session was handed back with no choice saved, so the owner answered in chat, verbatim: "So 2 is closest. \n\nBut I don't think you need separate settings pages, Just put surfaces over library in the same page.\n\nThen I don't think you need those grids in library anymore because surfaces already answers the "at a glance" view I wanted."

Decided:
- Direction 2 "Two pages" is the basis, made into one Settings page: Surfaces above, Prompt library below.
- Library rows lose their per-surface marks and grid, because the Surfaces section already gives the at-a-glance view.

## Look prompts-settings-v004, 2026-11-14 (owner)

The owner rejected v004. Their note, verbatim: "It's okay if the settings page scrolls we don't have to vertically squash the prompts library."

Decided:
- The v004 one-page layout stands, with one change: in a short window the Settings page scrolls. Surfaces does not fold, and the Prompt library keeps its full height.

The owner left two questions open, and the coordinator set defaults for them. The owner can change either.
- **"Any of these" with no condition holding:** the row is hidden only when every condition is of the hides kind. Otherwise it is greyed, and its tooltip gives the first greys-kind condition's reason.
- **Repository overrides:** may add, remove and reorder on a surface.

The layout is settled; the build waits on a written spec from the designer.

Owner, 2026-11-14 (after v004), verbatim: "btw I think our "system" prompts should also be in this prompt library but treated differently, no way to add new ones, no way to delete them only reset, and no way to add them to a surface"

Decided:
- The system prompts appear in the same Prompt library, marked as system. These are the instructions the app sends with an ask, today's "Built-in prompts" section.
- They can be edited and reset, but not added, deleted, duplicated onto a surface, or placed on one: drag and Add to are refused for them.
- The separate "Built-in prompts" settings section is removed.

Owner, 2026-11-14, verbatim: "disallow duplicate a system prompt". System prompts have no Duplicate.

## Spec questions, 2026-11-14 (owner, in chat)

Owner decisions on the questions the spec (SPEC.md (`docs/design/prompts-settings-v4/SPEC.md` on lane `claude/v0-5-0-prerelease-targets-52265d`)) raised:
- Action rows ("What changed since my review", "Explain this file", "Was this addressed?"): "Menu only, never chips". The thread-chip placement of "Was this addressed?" in the worked example is withdrawn.
- About is dropped: placement decides scope; a drop onto a surface whose scope cannot fill the prompt's placeholders is refused with a reason.
- Tour chats get surfaces: Tour stop, Tour part and Whole tour, under the same model.
- More is kept: the first 5 matching chips show, the rest go under More.

The coordinator set defaults for the rest; the owner can change them:
- The text an action row sends is a system prompt.
- Every prompt saves as it is typed, with Reset.
- The footnoted condition kinds stay as the spec reads them under Q1.

The card is ready to build.

## Claimed, 2026-11-14

Built from SPEC.md (`docs/design/prompts-settings-v4/SPEC.md` on lane `claude/v0-5-0-prerelease-targets-52265d`) with the
owner's answers above, in slices. Frontend-design loaded.
