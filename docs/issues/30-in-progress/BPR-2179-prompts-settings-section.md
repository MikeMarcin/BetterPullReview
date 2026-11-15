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

Owner decisions on the questions the spec ([SPEC.md](../../design/prompts-settings-v4/SPEC.md)) raised:
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

Built from [SPEC.md](../../design/prompts-settings-v4/SPEC.md) with the
owner's answers above, in slices. Frontend-design loaded.

### Slice 1: surfaces in core (2026-11-14)

`bpr/core/prompt_surfaces.hpp` (`core_prompt_surfaces`): nine surfaces (the
pull request, file and thread Ask menus and chips, and the owner's Tour stop,
Tour part and Whole tour), each an ordered list of placements, one prompt at
most once; the surfaces today's fields describe (`shipped_surfaces`: a quick
ask's scopes are its menus, a chip's place and About its chip surface, an
action row never a chip); placing refused with a reason for an action row on
a chip surface (the owner: menu only), a placeholder the surface cannot fill
(About dropped; placement decides), and a tour prompt off the tour's
surfaces; a placement placed again moves; remove and move within a surface.

### The plan for the rest (each its own commit)

2. Storage and host: a placements table beside the differences in
   `agent-setup.sqlite3`, read over `shipped_surfaces` (absent: shipped),
   with the revision rule; the host applies placements.
3. Decisions read the surfaces: the Ask menus' rows and order, and the chips
   (the first 5 of a surface's whose conditions hold, the rest under More, the
   owner keeping More).
4. The Settings page: a Prompts entry under Agents; Surfaces (three columns,
   each Ask menu and Suggested chips, and the tour chats), the legend, the
   Prompt library (filter, All / Built-in / Yours / Not on a surface / System,
   New prompt), the inline editor (name, Answer as, text with Insert and the
   byte count, conditions, Add to, Duplicate, Reset to default, "On N
   surfaces", Saved); selection outlining every placement; Add to menus;
   Option-Up/Down and Delete; drag within a surface and from the library.
5. Conditions with kinds (greys or hides) and the new built-in conditions,
   All of these / Any of these.
6. System prompts in the library (edit and Reset only; never placed,
   duplicated, added or deleted), the Built-in prompts section removed.
7. Repository overrides (the Prompts for pop-up and the second layer).
8. The look: screenshots and one Accept/Reject per criterion.

### Slices 2 to 7 (2026-11-14)

Frontend-design loaded for the page.

- **Storage (2).** `prompt_macro_differences::Surfaces`: each surface the
  reviewer changed, its list and what it took off, in `prompt_surface_lists`
  of `agent-setup.sqlite3` (layer `''`); with nothing stored a surface is
  what today's fields describe (`shipped_surfaces`), and a built-in a later
  release adds goes to the end of a changed surface. The merged set carries
  `Surfaces`; the write is under the existing revision rule.
- **Decisions read the surfaces (3).** `placed_prompt_macros` turns each
  placement into the copy the existing decisions read: placement decides the
  scope (a prompt about something narrower keeps its About and asks once for
  each). The Ask menus list in the surface's order (no longer action rows
  first); each chip surface has its own five, the rest under More. The Agent
  tab with no conversation shows the pull request's chips, then the file's
  (spec section 14 asked to confirm this; it changes today's order to Review
  this pull request, What could this change break?, Review <file>). A press on
  a menu-only prompt still sends it (`menu_prompt_offers`). An action on a
  menu narrower than its own scope is refused (`action_too_narrow`).
- **The page (4).** Settings › Prompts, after Agents & context
  (`prompts_page`): Surfaces (the three scopes' Ask menu and Suggested chips,
  then the tour chats), the legend, the Prompt library (filter, All /
  Built-in / Yours / Not on a surface / System, Restore built-ins…, New
  prompt) and the editor (name, Answer as, text with Insert and the byte
  count, conditions, Add to, Duplicate, Reset to default, "On N surfaces,
  outlined above", Saved). Selection outlines every placement with the link
  glyph; drag from the library and between surfaces (Option copies), Add to
  from the editor, a library row and a placement, Option-Up/Down, Delete, the
  menu key. Every change saves as made (typing after 450 ms), one write at a
  time. The page scrolls; the library body keeps 360 as drawn, and in a
  narrow window the editor goes under the list. Ask › Edit Quick Asks…
  opens it. The old Suggested prompts roster and its sheet are removed.
- **Conditions (5).** Kinds (`prompt_fact_kind`), All of these / Any of
  these (`prompt_match`), and the card's conditions: new pushes since my
  review, file has unresolved threads, I haven't reviewed this file, thread
  is unresolved, new replies or pushes since the comment (Count), replies
  since my comment. A menu row whose greys-kind condition is untrue is
  greyed with that condition's reason; hidden per Q1 and the Any default.
  Placement decides scope, so a review fact is allowed on every non-tour
  surface (unknown reads untrue).
- **System prompts (6).** The Built-in prompts section moved into the
  library under System, keeping its groups: edit, Reset, Preview, Test, `/`
  completion, byte count, Changed and Undo reset (its own editor, with Save
  and Discard as today: the spec's open saving question stays open). No drag,
  no on/off, Add to disabled with the spec's reason, no Duplicate. An action
  row's editor links to the system prompt it sends.
- **Repository overrides (7).** `prompt_repository_layer` (field overrides,
  its own prompts, each surface's added, removed and own order) in
  `prompt_repository_prompts` and `prompt_surface_lists`; the open pull
  request's repository's layer reaches its chips and menus. Prompts for lists
  All repositories, the open pull request's repository and those with a layer;
  the page shows Added here, Removed here (struck through), Off here, Only
  here, Changed here (with its filter and count), Order changed here with
  Revert, a teal dot and "Differs from app-wide" with each field's way back,
  and Use app-wide for this prompt. An app-wide prompt cannot be deleted here.

Open for the owner: the Agent tab's chip order above; the spec's footnoted
greys kinds; system prompts' Save/Discard against autosave; which
repositories Prompts for lists.

### Look prompts-v001 (filed 2026-11-14)

Session `c6fcc366221e664f5f4c37d884b0ec2c`, state
`/Users/jaeger/.attended-review-state/prompts-v001-2026-11-14/state` (URL in
`serve.log` beside it), cursor 36. Root `/Users/jaeger/prompts-v001.l5nlwa6h`
frozen from `738546b20` (tests-on, Fixture agent; its check answered all 17
asks). Twelve questions, one Accept/Reject each, with native off-screen
pictures from the root's own test executables: `BPR-2179-one-page`,
`-edit-obvious`, `-placing`, `-conditions`, `-system-prompts`,
`-repository`, `-agent-tab-chips`, and BPR-2151's `-pending-list`,
`-edit-hold`, `-stop-all`, `-follow-up-setting`, `-cmd-return`. AC03 waits on
the owner's verdicts.

### Look prompts-v001 handed back (owner, 2026-11-14)

Accepted, no note: `BPR-2179-one-page`, `BPR-2179-edit-obvious`,
`BPR-2179-placing`, `BPR-2179-repository`, `BPR-2179-system-prompts`,
`BPR-2179-agent-tab-chips` (so the pull request's chips, then the file's,
stands).

Rejected, `BPR-2179-conditions`, verbatim: "You should be able to nest
conditionals. The same level of boolean predicates we have in containers for
queries, although I don't think we need the entire containers query framework
thats probably more optimized than we need. /Users/jaeger/Documents/Personal/containers"

Its predicate model (the tag query editor's And, Or and Not groups over
predicates) was read; its query engine was not ported. Built:
`prompt_condition_group` (All of these, Any of these, None of these, nesting
two levels below the list, four conditions and four groups a level, twelve
conditions in all), decided recursively with each leaf keeping its greys or
hides kind and the All/Any rule (None: hidden only when every true leaf
hides; greyed with the true leaf's reason turned); the store's condition text
nests groups (an earlier build keeps such a row as a newer build's);
repository overrides carry groups; the editor draws each group as a bordered
box with its own match, Add (a condition or a group) and remove.

Coordinator's answers to the open items, on the accepted "saves as typed"
default: the chip order stands (accepted); the Conditions add button moved to
the heading's row with All/Any/None under it, so it is no longer cut; a
system prompt in the library saves as it is typed too (no Save or Discard
there; Reset and Undo reset stay).

### Look prompts-v002

Look `prompts-v002` filed (2026-11-14): session `a91caa31b1f548e1c54a22f0dc1350ec`, state `/Users/jaeger/.attended-review-state/prompts-v002-2026-11-14/state` (URL in `serve.log` beside it), cursor 15; root `/Users/jaeger/prompts-v002.bkog9vts` from `d92a6426e` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Five questions: `BPR-2179-nested-conditions`, `BPR-2151-queue-list-look`, `BPR-2151-drag-trash-pencil`, `BPR-2151-slow-fixture-ask` (type "Take your time"), `BPR-2151-thinking-for`.

### Look prompts-v002 handed back (owner, 2026-11-14)

Rejected, `BPR-2179-nested-conditions` ("very close"), verbatim: "None of
these problem doesn't make sense for top level, i can't imagine where it would
be useful. I don't undrestand the word "greys" here. The minus is not clear
that it is a delete, should be an x or trash can. It needs undo/redo to work.
And I would reduce the option to just Group in the Add since its so easy to
change between all/any/none. And it looks like we're narrower than we need to
be given hte layout of the dialog, seems like we have more room to work with
and the scroll bar makes it harder to read some stuff than it should."

Built: the prompt's own list offers All of these and Any of these only (None
is a group's); a condition's tag says Disable or Hide (what an untrue one does
to its menu row, in its tooltip), and the reasons say "disabled"; remove is a
trash can, for a condition and a group; Undo and Redo (buttons by the
heading, and the Undo and Redo keys there) step back through every edit of a
prompt, a run of typing being one step; Add offers Condition (a submenu) and
Group (a new group is All of these); the conditions column takes the room
beside the text (no fixed width) and has no scroll of its own, the editor
growing with them while the page scrolls.

### Look prompts-v003

Look `prompts-v003` filed (2026-11-14): session `2b1c044e69cb4b2740b13a50ad6c5915`, state `/Users/jaeger/.attended-review-state/prompts-v003-2026-11-14/state` (URL in `serve.log` beside it), cursor 9; root `/Users/jaeger/prompts-v003.dpoz1ap3` from `37f9eabf1` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Three questions: `BPR-2151-one-stop-one-time`, `BPR-2179-nested-conditions`, `BPR-2151-drawer-no-sideways`.

### Look prompts-v003 handed back (owner, 2026-11-14)

Rejected, `BPR-2179-nested-conditions`, verbatim: "I mostly like it. Use this
styling from overview instead of this uglier styling. Do that everywhere.
Disable on a condition still doens't make sense. Like what the heck does that
even mean in context of the groups or 'any of these'. I'm pretty sure that
needs to be rethought." (His pictures: the Overview's Open and All switch,
then the page's All of these / Any of these, Answer as and a group's
All / Any / None.)

The coordinator's proposal answering it, built: no kind on a condition. Each
prompt has one setting, "When they aren't met: Hide it / Show it disabled";
a disabled row gives the first unmet condition's reason (under None, the met
one, turned). A stored prompt from before migrates from its conditions' lean:
any condition that hid makes it Hide it, else Show it disabled; built-ins keep
what they did. The store writes the choice with the conditions (an earlier
build keeps such a row as a newer build's). Every choice on the page (the
library's filters, Answer as, All/Any, a group's All/Any/None, Hide it / Show
it disabled) and Settings › Agents & context's Follow-up behaviour are drawn
as the Overview's Open and All: the chosen one a quiet chip, the others their
words alone.

### Look prompts-v004

Look `prompts-v004` filed (2026-11-14): session `8a2a32701c0cb2bdeed967bb7585124e`, state `/Users/jaeger/.attended-review-state/prompts-v004-2026-11-14/state` (URL in `serve.log` beside it), cursor 15; root `/Users/jaeger/prompts-v004.mooeae56` from `7d17b1122` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Five questions: `BPR-2151-queue-one-line`, `BPR-2151-queue-steer`, `BPR-2151-one-stop-all`, `BPR-2179-choice-styling`, `BPR-2179-hide-or-disable`.
