# Prompts in Settings, round 4: surfaces over the library, on one page

Mockups for
[BPR-2179](../../issues/40-parked/BPR-2179-prompts-settings-section.md),
2026-11-14. In round 3
([prompts-settings-v3](../prompts-settings-v3/README.md)) the owner took
"Two pages" as closest and asked for two changes:

> But I don't think you need separate settings pages, Just put surfaces over
> library in the same page.
>
> Then I don't think you need those grids in library anymore because surfaces
> already answers the "at a glance" view I wanted.

This round draws that one layout. Nothing here is built, and the owner has
not yet judged it.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its three frames in both themes,
not screen captures. The prompts, the conditions' names and settings, and the
repository (`acme/checkout-api`) are invented.

## The page

Renders: all repositories [light](png/v4-app-light.png),
[dark](png/v4-app-dark.png); a repository [light](png/v4-repo-light.png),
[dark](png/v4-repo-dark.png).

- **One Settings page, Prompts.** Surfaces are on top, the Prompt library
  below.
- **Surfaces:** Pull request, File and Thread side by side, each with its Ask
  menu and its suggested chips. Each of the six is an ordered list, reordered
  by dragging. A small diamond marks a prompt that has conditions.
- **Prompt library:** a filter (a search field and All, Built-in, Yours, Not
  on a surface), a plain list, and a wide editor. A row is the on/off box, the
  name, and Yours or Edited. Rows carry no per-surface marks.
- **Where a prompt is:** selecting a prompt, or one of its placements,
  outlines all of its placements in Surfaces. The editor's footer counts them.
- **The editor** has the text at the left with Insert under it, and the
  prompt's conditions at the right: "All of these / Any of these", then one
  row per condition that opens to its own settings and is tagged greys or
  hides. The footer has Add to, Duplicate and Reset to default.
- **Adding:** drag a prompt up from the library onto a surface, right-click it
  and choose Add to, or use Add to in the editor.
- **A repository:** the "Prompts for" pop-up switches both parts. Teal marks
  what differs there: a placement Added here or Removed here, a prompt Off
  here or Only here, a dot by a prompt whose text changed. Each has a way
  back, and "Changed here" filters the library to them.

The rules drawn once in round 3 still hold: a chip shows only while its
prompt's conditions hold; an unmet condition greys the menu row with the
reason as its tooltip, or hides it, by its kind; each chip surface shows its
first 5 that match; a prompt turned off keeps its placements, dimmed and
skipped; only the reviewer's own prompts can be deleted, and deleting one
removes it from every surface after naming them.

## A short window

Renders: [light](png/v4-short-light.png), [dark](png/v4-short-dark.png).

**Surfaces folds; the page does not scroll.**

- When the window is too short for both parts, Surfaces folds to one row: the
  three scopes, each with an Ask menu well and a Chips well showing its count.
- The wells still take a drop, and the ones holding the selected prompt stay
  outlined, so adding and seeing where a prompt is both survive the fold.
  Right-click, Add to, is unchanged.
- The triangle by "Surfaces" opens and folds it by hand at any height. Opened
  in a short window, the surfaces scroll inside their own area.
- A scrolling page was the alternative. It was not chosen because it puts the
  surfaces out of sight exactly when a prompt is being dragged to them, and
  because the editor would move under the pointer while typing.

## Not decided

- With "Any of these" and none holding, which condition's kind and tooltip a
  menu row takes.
- Whether a repository may reorder a surface, or only add and remove.
- The settings each built-in condition has. "Count: Replies, Pushes" is only
  there to show a row with settings.
- The height at which Surfaces folds by itself.
