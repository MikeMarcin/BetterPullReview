# Prompts in Settings, round 3: surfaces, and a prompt library

Mockups for
[BPR-2179](../../issues/60-done/BPR-2179-prompts-settings-section.md),
2026-11-14. Round 2 ([prompts-settings-v2](../prompts-settings-v2/README.md))
ended with the owner choosing none of its directions and splitting the
section in two:

> First we have surfaces which defines where prompts appear. Then we have the
> prompt library where we can create new prompts, edit existing prompts, and
> we drag/right click from the prompt library to add to surfaces.

Nothing here is built. The owner's verdict is below.

## Owner's verdict (look prompts-settings-v003, 2026-11-14)

The owner handed the look back without choosing an option. Their note, saved
on the session as feedback and also given to the coordinator in chat,
verbatim:

> So 2 is closest.
>
> But I don't think you need separate settings pages, Just put surfaces over
> library in the same page.
>
> Then I don't think you need those grids in library anymore because surfaces
> already answers the "at a glance" view I wanted.

Direction 2 carries on as one page in
[prompts-settings-v4](../prompts-settings-v4/README.md). Directions 1 and 3
are not chosen.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its seven frames in both themes,
not screen captures. The prompts, the conditions' names and settings, and the
repository (`acme/checkout-api`) are invented.

## The two parts

- **Surfaces** say where prompts appear. There are six: the pull request,
  file and thread Ask menus, and the suggested chips of each. A surface is an
  ordered list of placements, reordered by dragging. A placement has no
  settings of its own.
- **The prompt library** holds every prompt: its name, its text with
  `{file}`, `{lines}` and `{thread}`, and its conditions. Prompts are made,
  duplicated, turned off and, for built-ins, reset there.
- A prompt goes onto a surface by dragging it there, or by right-clicking it
  and choosing Add to.

The worked example is "Was this addressed?": on the thread menu, the thread
chips and the pull request menu.

## Rules that hold in every direction

Render: [light](png/rules-light.png), [dark](png/rules-dark.png).

- **Conditions belong to the prompt.** They are edited in the library, as a
  list under an "All of these / Any of these" choice. Each condition is a row
  that opens to its own settings, after the WeakAuras trigger editor; only the
  interaction is borrowed, not the look. To offer a prompt under other
  conditions somewhere else, duplicate it and place the copy.
- **Each built-in condition has a kind,** shown on its row. "Greys" means the
  subject is there but the prompt cannot run yet (no pushes since my review):
  the Ask menu row stays, disabled, and its tooltip says what is missing.
  "Hides" means the subject is not there (no thread selected, a private note,
  a file a path rule leaves out): the row is not listed. This follows
  RFC-BPR-0128 Q1. A chip is offered only while the prompt's conditions hold.
- **Five chips per chip surface.** Each chip surface shows the first 5 of its
  own list whose conditions hold.
- **A prompt turned off keeps its placements.** They stay in place, dimmed,
  and are skipped: not in the menu, not a chip, not counted toward the 5.
- **Only the reviewer's own prompts can be deleted.** Deleting one removes it
  from every surface it is on, after a confirmation that names them.
  Built-ins are turned off or reset instead.
- **A repository starts from the app-wide surfaces and library.** A "Prompts
  for" pop-up switches both parts to one repository. Teal marks what differs
  there: a placement Added here or Removed here, a prompt Off here or Only
  here, a dot by a prompt whose text changed. Each has a way back to the
  app-wide value.

## Direction 1: Three panes

Renders: all repositories [light](png/d1-app-light.png),
[dark](png/d1-app-dark.png); a repository [light](png/d1-repo-light.png),
[dark](png/d1-repo-dark.png).

- One page: the six surfaces down the left, the library in the middle, the
  selected prompt's editor at the right.
- Drag a prompt left onto a surface. Each library row says its surfaces in
  words ("PR menu, Thread menu, Thread chips"). Selecting a prompt or a
  placement outlines the prompt's other placements.
- **Gives up:** the editor is the narrowest of the three, and the six
  surfaces share one column, so long surfaces make it scroll.

## Direction 2: Two pages

Renders: Prompt surfaces, all repositories
[light](png/d2-surfaces-light.png), [dark](png/d2-surfaces-dark.png); Prompt
library, a repository [light](png/d2-library-light.png),
[dark](png/d2-library-dark.png).

- Two entries in the Settings sidebar. Prompt surfaces has the three scopes
  side by side, with a shelf of the library under them to drag from. Prompt
  library has the list and the widest editor.
- On the library page each row has a mark per surface in three columns, and
  the editor lists the prompt's surfaces with Show.
- **Gives up:** ordering and editing are a page apart, and the shelf repeats
  the library.

## Direction 3: Source list

Renders: the library, all repositories [light](png/d3-library-light.png),
[dark](png/d3-library-dark.png); one surface, a repository
[light](png/d3-surface-light.png), [dark](png/d3-surface-dark.png).

- One page with its own short list at the left, as Music has a library and
  playlists: All prompts, then the six surfaces with their counts. The
  library view is the list and the editor. A surface opens as its ordered
  list, with a preview that answers which chips would show.
- Drag a prompt onto a surface's name.
- **Gives up:** the surfaces are never all visible together, and a dropped
  prompt lands at the end of a surface; its position is set after opening it.
  The preview is an addition nobody asked for.

## Not decided

- With "Any of these" and none holding, which condition's kind and tooltip a
  menu row takes.
- Whether a repository may reorder a surface, or only add and remove.
- The settings each built-in condition has. "Count: Replies, Pushes" is only
  there to show a row with settings.
