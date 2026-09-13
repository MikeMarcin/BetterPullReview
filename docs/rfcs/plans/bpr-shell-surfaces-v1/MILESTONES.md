# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr-shell-surfaces-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## intake-baseline

The pack is imported against the live source with allocated IDs, and the exact composition seams the design changes are recorded.

**Work:** `BPR-0770`

**Prerequisites:** None

**Completion:**

- Live HEAD, RFC and issue allocations and source drift are recorded without overwriting existing work.
- The Accounts adoption, last-runtime branch and quit policy are located in the live tree by file and function.

**Boundary:** One intake boundary; no application change is implied.

## accounts-page

Accounts & hosts is the title, the sentence, one card per server and one button, with sign-in inside the card.

**Work:** `BPR-0771`, `BPR-0772`, `BPR-0773`, `BPR-0774`

**Prerequisites:** intake-baseline

**Completion:**

- The retired objects are absent from the page and every composed course passes against its new target.
- Each configured host renders one card; sign-in, enterprise add, remember and forget work through the existing intents.

**Boundary:** One Settings-page boundary; Home and the chooser may proceed independently.

## home-page

Home is the bar, then state-ordered rows in collapsible tinted panels, with Group, State headers, account chips and one Filter menu in the strip.

**Work:** `BPR-0775`, `BPR-0776`, `BPR-0783`

**Prerequisites:** intake-baseline

**Completion:**

- First-launch, default, long-list, empty, over-a-review and the four grouping states render as accepted with the bar focused on arrival.
- Queue and recent rows share one delegate and panel; the surviving preferences live in Filter; view state persists per window; no empty box renders.

**Boundary:** One Home boundary; the queue projection and preference store are consumed unchanged.

## profile-chooser

Closing a window never presents the chooser, each platform gets its expected exit, and the chooser is a sheet of rows.

**Work:** `BPR-0777`, `BPR-0778`

**Prerequisites:** intake-baseline

**Completion:**

- The last-window course proves no presentation, the policy outcome per platform and single-owner reopening.
- The sheet component course proves rows, in-place editing, failure notice and the absence of retired controls.

**Boundary:** One desktop-lifecycle boundary; runtime admission and profile identity are unchanged.

## appearance

Each theme keeps its own colours, and the Appearance page is the theme strip, one-line type rows with a labelled preview, and colours for the theme on screen.

**Work:** `BPR-0789`, `BPR-0790`

**Prerequisites:** intake-baseline

**Completion:**

- Migration and per-variant commit, export, import and reset are proven with nothing lost.
- The six-tile strip, the eleven rows with glyphs, the preview and the 47-role list are composed in the real Settings window with the retired controls gone.

**Boundary:** One appearance boundary; RFC-BPR-0006 palettes, roles and theme documents are consumed unchanged.

## verification-closeout

Rendered, keyboard, screen-reader and attended evidence exists for all four surfaces, documentation matches, and formal status is truthful.

**Work:** `BPR-0779`, `BPR-0780`, `BPR-0781`, `BPR-0782`, `BPR-0791`

**Prerequisites:** accounts-page, home-page, profile-chooser, appearance

**Completion:**

- Captures, walks and attended verdicts are bound to revisions; unobserved platforms are recorded as not run.
- Documentation gate passes; every closeout row references evidence or a named unresolved criterion.

**Boundary:** One pack boundary; formal closeout follows canonical order and never falsifies a status.
