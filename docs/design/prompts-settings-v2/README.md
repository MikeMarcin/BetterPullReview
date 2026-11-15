# Prompts in Settings, round 2: one prompt in several places

Mockups for
[BPR-2179](../../issues/60-done/BPR-2179-prompts-settings-section.md),
2026-11-14, after the owner rejected layouts A, B and C (look ask-v005) and
the grill round settled the model. Nothing here is built, and no direction is
chosen yet.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its six frames in both themes,
not screen captures. The prompts, their conditions and the repository
(`acme/checkout-api`) are invented.

## What the mockups answer

The grill round left two things open:

1. How one prompt shows its several placements, in the list and in the
   editor.
2. How a repository's overrides look: what is overridden, and how to go back
   to the app-wide value.

Every direction draws the same case. "Was this addressed?" is in the thread
Ask menu and is a thread chip when there are new replies or pushes since the
comment. It is also in the pull request Ask menu, where one ask covers every
open thread. In `acme/checkout-api` its text has one more sentence and it is
also a pull request chip; that repository also turns "Draft a reply to this
conversation" off and has one prompt of its own.

Every direction keeps what the owner liked in round 1: C's filter (a search
field and All, Pull request, File, Thread), A's large edit area, B's drag to
reorder, and placement visible in the list without opening anything.
Built-ins can be edited, reset and turned off, never deleted. Conditions are
tokens picked from the fixed set, and all of a chip's conditions must hold.
Teal means "differs from app-wide" in all three.

## Direction 1: Grid

Renders: all repositories [light](png/d1-app-light.png),
[dark](png/d1-app-dark.png); a repository [light](png/d1-repo-light.png),
[dark](png/d1-repo-dark.png).

- **In the list:** one row per prompt. Pull request, File and Thread are
  fixed columns, each with a mark for the Ask menu and a mark for the chip. A
  prompt's placements read along its row; a column reads down as everything
  in that scope. A legend under the list names the two marks.
- **In the editor**, beside the list: the text, then the same grid at full
  size. One line per scope with an Ask menu box and a Suggested chip box, and
  the chip's conditions under the line.
- **A repository:** a "Prompts for" pop-up switches the whole section from
  All repositories to one repository. The list is then that repository's
  list. Each difference is ringed in teal (a mark in the grid, the on/off
  box, a dot by a name whose text changed), and "Changed here" filters to
  them. In the editor each changed value says what app-wide has and carries
  its own Use app-wide; the footer reverts the whole prompt.
- **Gives up:** the marks have to be learned. Conditions are not in the list.
- **Differs from the settled model:** there is no separate tick for a scope.
  A scope is ticked when either of its boxes is.

## Direction 2: Sentences

Renders: App-wide tab [light](png/d2-app-light.png),
[dark](png/d2-app-dark.png); a repository's tab
[light](png/d2-repo-light.png), [dark](png/d2-repo-dark.png).

- **In the list:** one row per prompt, with its placements written out under
  "Shows in": "Pull request menu, Thread menu and chip". No legend.
- **In the editor:** the row opens in place, the full width of the list. The
  text is at the left. At the right each scope is a ticked line ("On a
  thread") with its two places beneath, and the chip's line reads "Suggested
  chip, when all of these hold" over its condition tokens.
- **A repository:** tabs on the open prompt, App-wide and one per repository
  that changes it, with "+ Repository…" to add one. On a repository's tab the
  changed text is highlighted, the changed tick is ringed, each has Use
  app-wide, and one button removes all of that repository's changes. In the
  list a row names the repositories that differ ("Differs in…", "Off in…",
  "Only in…").
- **Gives up:** there is no view of one repository's whole list, so a
  repository cannot reorder prompts. Words scan more slowly than a column of
  marks. The open row pushes the rest of the list down.

## Direction 3: Surfaces

Renders: all repositories [light](png/d3-app-light.png),
[dark](png/d3-app-dark.png); a repository [light](png/d3-repo-light.png),
[dark](png/d3-repo-dark.png).

- **In the list:** there are three lists side by side, one per scope, each
  drawn as it shows: its Ask menu, then its suggested chips with each chip's
  conditions under it. A prompt appears once in every place it shows.
  Selecting one fills it and outlines the others with a link mark. Dragging a
  prompt onto another list shows it there too.
- **In the editor**, under the lists: the text at the left, and at the right a
  tab per scope whose label carries that scope's marks. The tab holds the two
  boxes and the chip's conditions.
- **A repository:** the "Prompts for" pop-up shows the lists as that
  repository will see them, with the differences tagged in place: Added here,
  Off here (struck through, with a revert), Only here. The editor opens with
  a strip that names each difference with its own revert, and Use app-wide
  for all.
- **Gives up:** there is no single list of prompts, so a prompt that is off or
  shows nowhere needs a shelf, which is not drawn.
- **Differs from the settled model:** each of the six lists has its own drag
  order. The grill round settled one order.

## What is the same in all three

- A window of 1160 by 840 points, the Settings sidebar with Prompts under
  Agents, and controls drawn as QMacStyle draws them (checkboxes, pop-up
  buttons, segmented controls; no switches).
- Insert `{file}`, `{lines}` and `{thread}` under the text, with the byte
  count; Answer as (A question, Findings); Reset to default and Duplicate.
- "One ask covers every open thread" is shown as a fact about the pull
  request scope, not as a setting.
