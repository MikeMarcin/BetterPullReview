# The import preview, three options

Mockups for the import preview of
[RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md),
built under
[BPR-2032](../../issues/60-done/BPR-2032-share-optional-workflow-configurations.md),
2026-11-15. Settings, Agents, Import… opens a configuration file and shows
what it would bring before anything is saved. In the look workflow-v002 the
owner rejected the window that ships, verbatim:

> Can we make Agents & context just say Agents in the settings. Why does this
> window look so different than everything else in the app? Why would leave
> out be the default? We're trying to improt it? Why does it take so many
> actions to choose between those 3 options? Why is it so hard to read what it
> does? We need to send this window back for a front end design pass.

Three options follow. Nothing here is built. The rename of the Settings
section to "Agents" is separate work; these frames already draw it.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its five frames in both themes,
not screen captures. The file drawn is the repository's fixture
`tests/fixtures/workflow_companion/synthetic-org.bpr-config.json`, with three
things invented so that every state shows: an earlier revision of the
workflow already on this computer (`9b1e44`, without the last action and
without the ticket change), a prompt that is the same as yours, and a second
workflow naming an action this version does not know. The revisions, the
second workflow, "Org linter" and "Claude Code (work)" are invented.

## What was wrong, and what all three do about it

| The owner asked | Today | In every option |
|---|---|---|
| Why does it look so different? | A tree with column headers, a splitter, a form and a table inside a bare dialog | Settings' own parts: grouped rows with the name at the left and the control at the right, the Prompts page's tags and segmented controls, the sheet an agent's settings open in |
| Why is Leave out the default? | Workflows and agents start left out; Import selected starts disabled | Everything the file can bring starts chosen. New is added, updated is updated. Import is enabled as the window opens |
| Why so many actions? | Select the row, open the pop-up, pick: three, per item | The choices sit on the item, all showing. One click picks one |
| Why is it hard to read? | Three table columns that cut off: "Gather the ti…", "You run synt…" | Whole sentences that wrap: New, Updated or Unchanged; which revision replaces which; who runs each action and with what; what each action says it does |

The choice has at most three parts, and their words change with what the item
is here:

- a new agent: **Add**, **Use _one of yours_**, **Leave out**;
- an agent you already have: **Update**, **Use _one of yours_**, **Leave out**;
- a workflow: **Add** or **Update**, and **Leave out**;
- an unchanged item has no choice, and neither does one that cannot be
  imported.

The one choice that takes two actions: when two or more of your agents could
stand for the file's agent, the middle part names one and opens a menu of
them.

## RFC-BPR-0112's rules, as drawn

- **No program path travels.** An action bound to a program shows the name
  the file gives (`synthetic_org_companion.py`) and "is not chosen yet", with
  Choose program… beside it. A custom agent says the same of its program and
  is left out until it has one. Import does not wait for the workflow's
  program: its actions cannot be handed off until it is chosen.
- **An unknown action is named and refused.** The second workflow sits under
  "Cannot be imported" with the name it used, "deploy_preview", and no
  choice. The rest of the file still imports.
- **An update is compared before it is taken.** The update says which
  revision replaces which, and its differences are marked on the actions they
  belong to ("New action", "Does more", with the added effect in the caution
  colour) before Import is pressed. Drafts that selected the earlier revision
  keep it until the update is accepted there.
- **Importing runs nothing and changes no credential.** The sheet's first
  lines and its last say so.
- **Builds stay off without acceptance** (AC09). "Let Writer build and run
  tests" is the one thing a file can ask for that is not chosen by default.
  It is a permission, not an item of the file.
- **The agent accent.** An action bound to an agent wears it; one bound to a
  program does not.

## A. One sheet

Renders: as it opens [light](png/a-light.png), [dark](png/a-dark.png); the
other choices [light](png/a2-light.png), [dark](png/a2-dark.png).

A sheet over Settings, as an agent's settings are. Every item in the file is
a group in one column: its name, what it is here, its choice at the right,
and what it does written under it. A workflow's group goes on to its actions,
its program and the line about drafts. There is no selection and no second
pane.

- **For.** Nothing is hidden behind a selection. The ordinary file, one
  workflow and its agent, reads top to bottom in one pass, and Import needs
  no click before it.
- **Against.** It is tall. The frame is drawn in a tall window so the whole
  file shows; in a shorter one the column scrolls between the title and the
  buttons. A file with many agents or prompts scrolls further, and a changed
  prompt's text has no room to sit beside yours: its comparison would open
  under its row.
- **To build.** Least. What the preview decides does not change. The tree,
  the splitter and the stacked detail pages become one scrolling column of
  groups Settings already draws.

## B. List and detail

Renders: as it opens [light](png/b-light.png), [dark](png/b-dark.png); the
agent selected [light](png/b2-light.png), [dark](png/b2-dark.png).

The Prompt library's shape in a sheet. At the left a ticked list of the
file's items, each with its tag and one line saying what happens. At the
right the selected item in full. A tick is the choice to import; an agent's
Add or Use is a pair of radio buttons in its detail. An update is shown as
yours beside the file's, row against row, with the added action tinted and
the widened effect marked.

- **For.** It holds a large file: twenty prompts are twenty short rows. The
  comparison has room.
- **Against.** One item's detail shows at a time, so reading everything means
  clicking through the list. An agent's choice is in two places. It is the
  nearest of the three to the window that was rejected: the same two panes,
  redrawn.
- **To build.** Middling: the list and detail as today in Settings' parts,
  plus the side-by-side action comparison.

## C. In the page

Renders: [light](png/c-light.png), [dark](png/c-dark.png).

No window. The file opens into the Agents page. A banner says what is being
imported, counts what it adds and updates, names what cannot be imported, and
holds Cancel and Import. The file's agent and workflow are drawn in the
page's own lists, where they will live, with a blue edge until they are
saved. Their rows are A's rows.

- **For.** It cannot look unlike Settings, because it is Settings. An update
  is seen on the row it replaces, beside the agents you already have.
- **Against.** The page gains a mode. Leaving the page, searching settings,
  or opening an agent in the middle of an import each need an answer. Prompts
  live on another page, so the banner can only mention them. In an ordinary
  window the workflow runs below the fold, as drawn.
- **To build.** Most. The Agents page has to draw unsaved rows among saved
  ones and hold them until Import or Cancel.

## Recommendation

**A.** It answers each of the owner's four questions in the plainest way, it
is the shape the application already uses for an agent's settings, and it
asks the reader only to read down and press Import. B's yours-beside-the-file
comparison can be taken later, inside A's workflow group, if large files turn
out to be common. C is the most at home in Settings and costs a page mode
that nothing else needs.

## To settle when it is built

- **Update by default, and BPR-2032's evidence.** The card's AC02 evidence
  says the preview "takes an update only when chosen", and today that is a
  choice away from Leave out. Here the update starts chosen and the choosing
  is pressing Import with the comparison in view. The RFC's sentence, "a
  configuration update is compared and accepted", still holds; the test that
  asserts the old default has to change with it. If the owner wants a
  widened effect ("Does more") to need its own tick, that is one more
  checkbox on that action and no change to the layout.
- **One program for the actions that name it.** Today a program is chosen
  for one selected action at a time. These frames choose it once for every
  action naming it, which still binds each action explicitly. Whether a
  single action may then be given a different program in the preview is not
  drawn.
- **An agent left out while its workflow comes in.** The frame says the
  action that asks for it "waits until an agent stands for it". What the
  core does in that case was not checked for this round; the sentence must
  follow the core.
- **Not drawn:** a changed prompt's comparison, a file that is refused whole
  (newer version, not a configuration file), hover, focus and pressed
  states, the Windows and Linux styles, and the Export dialog, which has the
  same tree and would want the same treatment.

## How the renders were made

Chrome's headless mode on a throwaway profile, one frame and theme at a time
(`?theme=dark&frame=a`), sized to the frame at 1.5 times. `frame` is `a`,
`a2`, `b`, `b2` or `c`. The controls are drawn after the application's macOS
style as the earlier Settings mockups drew it
([prompts-settings-v4](../prompts-settings-v4/README.md),
[BPR-2051](../BPR-2051-agents-roster-and-footer/README.md)); they are not
the application's own widgets.

## Status

Filed for the owner on 2026-11-15 as attended review `workflow-import-v001`:
one question, a choice between A, B and C. External state directory
`~/.attended-review-state/workflow-import-v001-2026-11-15/` (`state/` is the
session and holds its ID). Lifecycle: awaiting the owner's answer. Nothing is
built until it is given.
