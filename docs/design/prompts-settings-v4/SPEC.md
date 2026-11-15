# Prompts in Settings: build spec

For the implementer of
[BPR-2179](../../issues/60-done/BPR-2179-prompts-settings-section.md).
The layout is the one drawn in this folder
([all repositories](png/v4-app-light.png),
[a repository](png/v4-repo-light.png)), with the owner's change from look
prompts-settings-v004: in a short window the page scrolls.
[README.md](README.md) has the history.

Each statement has one of five standings. Where a section does not say, it is
**drawn**.

| Standing | Meaning |
|---|---|
| Decided | The owner said so. |
| Default | The coordinator set it; the owner may change it. |
| Drawn | It is in the frames the owner judged in look v004. Their layout stands, apart from scrolling. |
| Proposed | The frames and the card do not settle it. This is the designer's fill-in; confirm before building. |
| Today | How the code behaves at lane commit `c1c1f512e`. |

## 1. The two parts

- **Surface:** a place prompts appear. There are six: the pull request, file
  and thread Ask menus, and the suggested chips of each. A surface is an
  ordered list of placements. (Decided)
- **Placement:** one prompt on one surface. It has a position and nothing
  else. A prompt is on a surface at most once. (Decided; "at most once" is
  proposed)
- **Prompt library:** every prompt, with its name, text, Answer as, conditions
  and on/off. (Decided)
- A prompt is hidden from a surface only by not being placed on it.
  Conditions decide whether a placed prompt is offered. (Decided)
- The library also holds the **system prompts**, which are never placed.
  Section 10 covers them; everything before it is about placeable prompts.
  (Decided)

## 2. Layout and sizes

Sizes are in points as drawn. Native controls keep the size QMacStyle gives
them; the numbers for them are what that came to in the frames.

### Page

| Item | Size |
|---|---|
| Window as drawn | 1180 by 840 |
| Settings sidebar | 196 wide; one entry, Prompts, under Agents |
| Page padding | 13 top, 28 left and right, 16 bottom |
| Content width | 928 at the drawn window width |
| Title row | 26 high; "Prompts" 16 bold; "Prompts for" label and pop-up at the right |

Top to bottom: title row; Surfaces heading; the three scope columns; the
legend line; the library tools row; the library body.

### Surfaces

| Item | Size |
|---|---|
| Heading row | 22 high, 7 below; "Surfaces" 13 semibold, then a caption, 12, secondary |
| Columns | Three, equal width, 12 apart (301 each as drawn); a grouped panel, radius 9, padding 8 |
| Scope name | 13 semibold: Pull request, File, Thread |
| List heading | 11.5 semibold, secondary: "Ask menu"; "Suggested chips", then "up to 5 show" in tertiary |
| Placement row | 22 high, radius 6, padding 0 6 0 4, 6 between items |
| Row contents | Grip 8 by 14; then for a menu the Ask sparkle 13 and the name 13; for a chip the name in a capsule 19 high, radius 10.5, padding 0 8, text 12 |
| Row's right end | Condition diamond 9; tags 16 high, text 10.5; revert button 20 by 16; link glyph 12 |
| Legend line | 11, secondary, 7 above: the diamond and "Has conditions. Its chip shows only while they hold; its menu row is greyed or hidden while they do not." |

- The caption reads "Where prompts appear, in order. Drag a prompt up from the
  library, or right-click it and choose Add to." For a repository it reads
  "`owner/name` starts from your prompts for all repositories. Changes here
  apply to its pull requests only."
- The frames draw a disclosure triangle before "Surfaces". Leave it out:
  Surfaces does not fold. (Decided, look v004)
- A column is as tall as its lists. Surfaces has no scroll bar of its own.

### Prompt library

| Item | Size |
|---|---|
| Tools row | 13 above, 9 below, 24 high |
| Tools | "Prompt library" 13 semibold; search field 168 by 22, "Filter prompts"; segmented All, Built-in, Yours, Not on a surface (and System, section 10, not drawn; the row is nearly full as drawn, so the search field gives way); for a repository a "Changed here" checkbox with its count; New prompt at the right |
| Body | List and editor side by side, 12 apart, 360 high (proposed: fixed, see Scrolling) |
| List | 272 wide; rows 26 high: on/off checkbox 15, name 13, then tags 15 high |
| Editor | The rest (644 as drawn); a grouped panel, padding 13 14 12 |
| Editor header | Name field 300 by 28, text 14.5 semibold; a Built-in tag; "Answer as" and a segmented A question, Findings at the right; 10 below |
| Editor columns | Two, 18 apart. Left: the text, flexible. Right: conditions, 328 wide |
| Text area | Fills the left column; monospace 11.5 on 17, padding 8 10, radius 6 |
| Insert row | 18 high, 7 above: "Insert", the variables as small tokens, and "N of 8,192 bytes" in tertiary |
| Conditions | Header 24 high: "Conditions" 13 semibold, segmented All of these, Any of these, add button 26 by 20 at the right. Rows 26 high. An open row adds its settings, indented 15, lines 22 high, and a note 11.5 on 15 |
| Footer | 10 above, buttons 24 high: Add to (pop-up), Duplicate, Reset to default; at the right "On N surfaces, outlined above" and "Saved" |

Library rows carry no per-surface marks. (Decided, look v003)

### Colour

Use the app's existing tokens for text, grouped panels, lists, selection, the
accent and the Ask sparkle. One colour is new: the override colour, meaning
"differs from app-wide".

| Token | Light | Dark |
|---|---|---|
| Override text and edges | `#0b7672` | `#4fd0c9` |
| Override tint (tags, buttons) | 10% of it | 13% of it |

No state is shown by colour alone: every teal mark has a word beside it, and
an outlined placement has a link glyph.

## 3. Scrolling

- The Prompts page scrolls vertically as one page when the window is too short
  for it. Surfaces does not fold and the library is not squashed. (Decided,
  look v004: "It's okay if the settings page scrolls we don't have to
  vertically squash the prompts library.")
- The library body keeps its full height, 360 as drawn, at every window
  height. (Decided that it keeps full height; the number is drawn)
- Inside the library body, the list scrolls when it has more rows than fit
  (13), and the text area scrolls when the text is longer than it. Nothing
  else scrolls on its own. (Proposed)
- While a prompt is dragged, holding it near the top or bottom edge of the
  page scrolls the page, so a surface that is out of view can be reached.
  (Proposed)
- There is no horizontal scroll bar. The three columns share the width; names
  elide at the end. (Proposed)

## 4. Selection

- One prompt is selected at a time. The editor always shows it; nothing needs
  a hover or a double-click (card AC02). (Drawn)
- Clicking a library row selects that prompt and outlines every placement of
  it: a 1.5 accent outline and the link glyph.
- Clicking a placement selects its prompt, fills that placement with the
  accent and outlines the prompt's other placements.
- The editor footer says "On N surfaces, outlined above".
- Changes save as they are made; the footer shows "Saved". There is no Save
  button. (Drawn)

## 5. Adding, moving and removing placements

**Drag from the library.** (Decided)

- Any library row can be dragged. The row dims; a ghost follows the pointer
  with a green plus badge.
- Over a surface an insertion line (2, accent, a ring at its left) shows where
  it will land. Dropping adds the placement there.
- If the prompt is already on that surface, the drop moves it to the new
  position. (Proposed)
- A surface that cannot take the prompt shows no insertion line and the
  not-allowed cursor. (Proposed; see "Which surfaces a prompt can go on")

**Drag within Surfaces.**

- Dragging a placement inside its surface reorders it. (Decided)
- Dragging it onto another surface moves it there; with Option held it is
  copied. (Proposed)

**Add to.** The same menu in three places: right-click a library row and
choose Add to (Decided); the Add to pop-up in the editor footer (Drawn); and
right-click a placement (Proposed).

- It lists the six surfaces under Pull request, File and Thread: "Ask menu"
  and "Suggested chips" under each.
- A tick marks a surface the prompt is on. Choosing an unticked surface adds
  the prompt at its end; choosing a ticked one removes it. (Drawn)
- A surface that cannot take the prompt is disabled. (Proposed)

**Removing.** Untick it in Add to; or select the placement and press Delete;
or right-click the placement and choose "Remove from Thread chips" (with the
surface's name). Removing a placement never deletes the prompt. (Proposed)

**The library row's menu:** Edit (selects it and puts the caret in the text),
Add to, Duplicate, Turn Off or Turn On, Reset to Default (built-ins that
differ), a separator, Delete… (own prompts only). (Drawn, except Reset to
Default, proposed) A system prompt's menu is shorter; see section 10.

### Which surfaces a prompt can go on

Not settled by the card. Today a macro has one About and may be asked at that
scope or a wider one, where it asks once for each (`prompt_scope_allowed`).
Keep that rule: a prompt about a thread can go on the thread, file and pull
request surfaces; one about a file on the file and pull request surfaces; one
about the pull request on the pull request surfaces only. (Proposed)

Two things need a decision before this can be built:

1. **About.** The editor drawn has no About control. Either About follows from
   the variables the text uses, or the editor needs an About pop-up.
2. **Action rows as chips.** Today "What changed since my review", "Explain
   this file" and "Was this addressed?" are action rows, and an action row is
   never a chip. The owner's worked example puts "Was this addressed?" on the
   thread chips, so that rule has to go.

## 6. The editor

- **Name:** one line, up to 120 bytes (today's bound).
- **Answer as:** A question, or Findings. Findings is today's review kind and
  keeps its limits (about the pull request or the open file).
- **Text:** up to 8,192 bytes (today's bound); the count is under it.
- **Insert:** offers the variables the prompt's surfaces can fill (today's
  `prompt_variables`), not only the three drawn. `{thread}` is drawn but is
  not a variable today.
- **Duplicate:** makes one of the reviewer's own prompts named "`name` copy",
  with the same text, Answer as and conditions, on no surface. It is selected
  and its name has the focus. This is how a prompt gets different conditions
  somewhere else. A system prompt cannot be duplicated. (Decided that
  Duplicate is the way, and that system prompts have none; the details are
  proposed)
- **Reset to default:** built-ins only; enabled when the prompt differs from
  the shipped one. It restores the shipped name, text, Answer as and
  conditions. It does not change on/off or any placement. (Decided that
  built-ins have it; the scope of the reset is proposed)
- **New prompt:** makes one of the reviewer's own prompts named "New prompt",
  with no text and no conditions, on no surface, selected, with the name
  focused. (Proposed)

Today a built-in changed against an older shipped revision offers Compare.
Keep it; it is not drawn.

## 7. Conditions

Conditions belong to the prompt, not to a placement. (Decided)

### Editing

- The list sits under a choice of "All of these" or "Any of these". A new
  prompt has All of these. (Decided)
- The add button opens a menu of the built-in conditions, grouped Pull
  request, File, Thread, Chat. Free-form rules are out of scope. (Decided
  that the set is fixed; the grouping is proposed)
- Each condition is a row: a disclosure triangle, its name, its kind as a
  small tag ("greys" or "hides"), and a remove button. Opened, the row shows
  its own settings and one line saying what happens while it is unmet. Rows
  open and close independently; a row just added is open. (Decided: rows with
  their own settings, after the WeakAuras trigger editor, and the kind shown
  on each. The rest is drawn or proposed)
- A prompt holds up to 4 conditions (today's bound). At 4 the add button is
  disabled and its tooltip says so. (Proposed)

### What they do

A prompt's conditions **hold** when, under All of these, every one is true;
under Any of these, at least one is true. With no conditions they always hold.
A fact the window does not know yet makes its condition untrue (today's rule).

| Surface | Conditions hold | Conditions do not hold |
|---|---|---|
| Suggested chips | Offered, if it is among the first 5 | Not offered (Decided) |
| Ask menu | Listed, enabled | Hidden or greyed, by kind, below |

The kinds follow RFC-BPR-0128 Q1 (Decided):

- **Hides:** the condition is about whether the subject is there (no thread
  selected, a path rule that leaves the file out).
- **Greys:** the subject is there but the prompt cannot run yet (no commits
  since my review, no failed check, no threads on the file). The row is
  listed, disabled; its tooltip and its accessible description give the
  reason.

In an Ask menu, when the conditions do not hold:

- **All of these:** if any untrue condition is of the hides kind, the row is
  hidden. Otherwise it is greyed, with the reason of the first untrue greys
  condition in the list's order. (Proposed)
- **Any of these:** the row is hidden only if every condition is of the hides
  kind. Otherwise it is greyed, with the first greys condition's reason.
  (Default)

Unchanged from today: a built-in action's own needs and reasons come before
the prompt's conditions; an agent that is busy or not set up never greys a
row; a prompt placed at a wider scope asks once for each thread or file that
meets its conditions, and is greyed when none does.

Today an unmet condition always hides the row. Kinds are new in core.

### The built-in conditions

Nine are today's condition facts (`prompt_fact`), named in the last column.
The rest come from the card. A condition's kind is the same whichever way it
is written ("is" or "is not").

| Condition | Group | Kind | Its settings | Reason shown when greyed | In core today |
|---|---|---|---|---|---|
| Pull request is mine | Pull request | Greys ¹ | is, is not | "This is your pull request." / "This is not your pull request." | `author_is_me` |
| Pull request is a draft | Pull request | Greys ¹ | is, is not | "This pull request is a draft." / "…is not a draft." | `draft` |
| A check failed | Pull request | Greys | is, is not | "No check has failed." | `checks_failing` |
| Repository matches | Pull request | Hides | patterns; matches, doesn't match | | `repository` |
| New pushes since my review | Pull request | Greys | none | "No commits since your review."; with no review yet, "You haven't reviewed this pull request yet." (both today's strings) | the What changed action's own need |
| A file is open | File | Hides | is, is not | | `file_open` |
| File path matches | File | Hides | patterns; matches, doesn't match | | `path` |
| Lines are selected | File | Hides | any, one line, several lines; is, is not | | `selected_lines` |
| File has unresolved threads | File | Greys | is, is not | "No unresolved threads on this file." | new |
| I haven't reviewed this file | File | Greys ¹ | none | "You have already reviewed this file." | new |
| Thread takes replies | Thread | Hides | is, is not | | `takes_replies` |
| Thread is unresolved | Thread | Greys | is, is not | "This thread is resolved." | new |
| New replies or pushes since the comment | Thread | Greys | Count: Replies, Pushes (both ticked) | "No replies or pushes since the comment." | pushes: the Was this addressed? action's own need; replies: new |
| Replies since my comment | Thread | Greys | none | "No replies since your comment." | new |
| Chat is empty | Chat | Hides | is, is not | | `empty_chat`; read by chips only |

¹ The subject is there, so the rule gives greys; but these read more as "not
for this case" than "not yet", and greyed rows for them may be noise. Confirm
with the owner.

Notes on the table:

- The kinds of "New pushes since my review", "A check failed", "File has
  unresolved threads" and "File path matches" follow the RFC's own examples.
  The others are the designer's reading of the rule.
- The reasons marked as today's strings exist in `ask_menu.cpp`. The others
  are suggested wording.
- "Thread takes replies" is drawn in the frames as "Not a private note".
- "Count: Replies, Pushes" is the only setting that is not an existing
  polarity or value. It is proposed.

## 8. The five-chip cap

- Each chip surface offers the first 5 of its own list whose conditions hold,
  in the list's order. The three chip surfaces each have their own 5.
  (Decided)
- A chip surface may hold more than 5 placements. Settings draws no line after
  the fifth, because which 5 show depends on the conditions.
- A prompt that is off, or removed in the repository, takes no place.
- Today matches after the fifth go to More (`maximum_prompt_chips`). The card
  does not mention More. Keep it unless the owner says otherwise. (Proposed)

## 9. Turned off, and deleted

**Turned off.** The checkbox on a library row, or Turn Off in its menu.

- Built-ins and the reviewer's own prompts can both be turned off. (Decided
  for built-ins)
- The prompt's placements stay where they are, dimmed, with an "Off" tag.
  They are skipped: not in the menu, not a chip, not counted toward the 5.
  Turning it on brings them back in the same positions.
- An off prompt can still be edited, placed and reordered.

**Deleted.** Delete… in the library row's menu, or Delete with a library row
selected.

- Only the reviewer's own prompts can be deleted. Built-ins have no Delete…
  item. (Decided)
- A confirmation names the surfaces: "Delete “`name`”?" and "It is on 2
  surfaces and will be removed from them: File menu and File chips. This
  cannot be undone." With none: "It is not on any surface. This cannot be
  undone." Buttons: Cancel, Delete.
- Deleting removes the prompt and every placement of it.

## 10. System prompts

The owner, 2026-11-14, after look v004, as relayed by the coordinator:

> btw I think our "system" prompts should also be in this prompt library but
> treated differently, no way to add new ones, no way to delete them only
> reset, and no way to add them to a surface

and, on duplicating one: "disallow duplicate a system prompt".

System prompts are what today's Built-in prompts section of Settings holds:
the instructions the app sends with an ask, listed under Tour tab, Comments
and notes, Local drafts and Agent tab, and the Review skill. None of this is
drawn; the frames show placeable prompts only.

| | Placeable prompts (built-in and the reviewer's own) | System prompts |
|---|---|---|
| In the library | Yes | Yes, in the same list (Decided) |
| Marked | "Yours", "Edited", or nothing | A "System" tag (Decided that they are marked; the tag is proposed) |
| New | New prompt makes one | Never. The set is fixed (Decided) |
| Edit the text | Yes | Yes (Decided) |
| Reset to default | Built-ins | Yes (Decided) |
| Delete | The reviewer's own | No (Decided) |
| Duplicate | Yes | No (Decided) |
| On a surface | By drag or Add to | Never (Decided) |
| Conditions | Yes | None |
| On/off checkbox | Yes | None (Proposed) |
| Answer as | Yes | Not shown (Proposed) |

- **Where they sit.** After the placeable prompts, under a "System" heading
  in the list, keeping today's four groups as sub-headings. The filter gains
  a "System" segment. "All" shows both kinds; "Not on a surface" leaves
  system prompts out, since they can never be on one. (Proposed)
- **Dragging.** A system row does not start a drag. Its menu shows Add to
  disabled, with the tooltip "System prompts are sent by the app and can't be
  added to a surface." Its menu has Edit and Reset to Default only.
  (Decided that neither is possible; showing why this way is proposed)
- **The editor.** The name is shown but not editable. The right column has no
  conditions; it carries today's two lines, what the prompt is for and where
  it is used. The footer has Reset to default and no Add to, Duplicate or
  surface count. (Proposed)
- **Kept from today's section:** Preview of what a run receives, Test saved
  prompt, `/` skill completion, the byte count against its own bound, the
  "Changed" marker and Undo reset. (Proposed)
- **Counts.** System prompts are not counted as the reviewer's own, nor
  against the bound on how many prompts a profile holds. (Decided that they
  are not counted as new; the bound is proposed)
- **The Built-in prompts section goes away** once the library holds them.
  (Default)

Two things here are not settled:

1. **Saving.** Today a system prompt keeps unsaved edits until Save or Discard
   changes. The frames draw the other prompts saving as they are typed. One
   editor should not do both without saying so.
2. **Repositories.** Today a system prompt is saved for the profile. Whether a
   repository can override one is not decided. Proposed: no; they are edited
   under All repositories only.

## 11. Repository overrides

Storage has two layers: the app-wide prompts and surfaces, and each
repository's differences from them. (Decided)

**Choosing the layer.** The "Prompts for" pop-up at the top right has "All
repositories" and the repositories. It switches both Surfaces and the
library. With a repository chosen the pop-up has a teal edge. What is edited
then is stored for that repository only. At run time a pull request uses its
own repository's layer. (Drawn; which repositories the pop-up lists is not
settled)

**What a repository can change.**

| Part | Change | How it shows | The way back |
|---|---|---|---|
| Surface | Add a placement | "Added here" tag on the row | The revert button beside the tag removes it |
| Surface | Remove an inherited placement | The row stays, struck through and dimmed, with "Removed here" | The revert button restores it |
| Surface | Reorder (Default: allowed) | Not drawn. Proposed: an "Order changed here" tag on the list heading | Proposed: a revert button beside that tag restores the app-wide order |
| Prompt | Name, text, Answer as or conditions | A teal dot by the name, in the library and on its placements. In the editor the changed field has a teal bar, and under it "Differs from app-wide.", "Show app-wide text" and Use app-wide | Use app-wide, for that field |
| Prompt | Turn it off | "Off here" tag; a teal ring on its checkbox; its placements dim with "Off here" | Tick the checkbox |
| Prompt | A prompt of the repository's own | "Only here" tag | Delete it |

- In a repository the editor footer has "Use app-wide for this prompt" in
  place of Reset to default. It drops every difference the repository has for
  that prompt. It does not touch placements.
- "Changed here" in the tools row filters the library to prompts that differ;
  its count is how many do.
- Each of name, text, Answer as, conditions (with the All or Any choice) and
  on/off is overridden on its own. A field the repository has not changed
  follows later app-wide edits. (Proposed)
- A repository that has reordered a surface keeps its own order for it. A
  placement added app-wide afterwards goes to the end there. (Proposed, after
  today's merge rule for built-ins added by a later release)
- An app-wide prompt cannot be deleted from a repository; turn it off there.
  (Proposed)

## 12. Keyboard and assistive technology

All proposed; the frames do not show focus.

- Every control, library row and placement is reachable by Tab and the arrow
  keys, with a visible focus ring.
- Option-Up and Option-Down move the selected placement within its surface.
  Delete removes the selected placement. The menu key or Control-click opens
  the row's menu, so Add to never needs a drag.
- A placement is named with its surface, position and state: "Thread chips, 1
  of 4, Was this addressed?, has conditions". Off, Added here, Removed here
  and Off here are part of the name.
- A greyed Ask menu row's reason is its accessible description (RFC-BPR-0128
  AC03, AC08).

## 13. Who decides what

The core decides. The host performs. Qt presents.

- **Core:** whether a prompt's conditions hold, each condition's kind, hidden
  or greyed and the reason, the first 5 chips, each surface's order, which
  surfaces a prompt may go on, and the merge of a repository's differences
  over the app-wide layer. All of it pure decisions over facts.
- **Host:** gathers the facts a window already holds, and stores the two
  layers.
- **Qt:** the page, the drag and drop, the menus. A drop or a menu choice is
  an intent handed inward. No widget reads or writes storage.

## 14. Not covered by this design

Decide these before or during the build; none is drawn.

- **The tour chats.** Today's macros also have the tour stop, tour part and
  whole-tour places, with thirteen built-in chips. The six surfaces do not
  include them. Either they get surfaces, or they stay outside this page.
- **The Agent tab's chips.** This spec takes the pull request and file chips
  to be the Agent tab's chips with no conversation attached, and the thread
  chips to be its chips with one attached. Confirm.
- **What an action row sends.** Today "What changed since my review",
  "Explain this file" and "Was this addressed?" carry a name and where they
  show; what they send is one of the Built-in prompts. The frames draw "Was
  this addressed?" with its text in its own editor. Either an action row's
  editor shows and edits that system prompt's text, or it links to it.
- **Restoring the shipped surfaces.** Reset to default restores a prompt, not
  where it is placed. No control puts the surfaces back as shipped.
- **Which repositories "Prompts for" lists.**
- **Sharing.** Import and export of edited prompts (RFC-BPR-0107) is not
  touched here.
- **Ask > Edit Quick Asks…** opens this page (card AC02). Which prompt it
  selects is not settled.
