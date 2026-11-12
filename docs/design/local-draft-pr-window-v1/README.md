# Local draft in the pull request window: accepted design

Accepted by the owner on 2026-11-12. This folder is the accepted design record
for the local-draft redesign on lane `claude/v0-5-0-prerelease-targets-52265d`
(mandate `v0-5-0-prerelease-targets-r1`). It replaces the local-draft page,
its Description, Tour, candidate and Pull request areas and Home's In
preparation band, which the owner rejected in the looks of 2026-11-11 and 12
(verdicts on BPR-2021, BPR-2023, BPR-2024 and the RFC-BPR-0108 and 0110
ledgers).

Every page is standalone HTML with a Light and Dark link at its top right; the
PNGs are headless renders of the same pages, downscaled, not screen captures.
Content is invented.

## The owner's choices, verbatim

1. Mockup A and Home variant 1, with author state (2026-11-12):

   > So I think A + Home 1, but A needs a little bit of extra love for being
   > able to edit locally. Local draft is more of a state than a mode since
   > we're talking about letting this same mode be used for addressing PR
   > feedback. Maybe that takes the form of a no-diff view option (like we
   > have 2 pane and single pane) maybe instead of the 1 button toggle this
   > current is it becomes a 2-icon pick 1 normally and a 3 icon-pick one if
   > you're in author/prepare mode. And then we probably need a better way to
   > see changes. There are a few kinds that matter. Changes relative to merge
   > base, changes relative to PR head, and local changes not yet committed
   > even locally. I dunno the best way to show these, perhaps it's a new tab
   > in the file/find-in-files drawer.

2. Staging, commit and push (2026-11-12), with Fork's Local Changes view as
   the inspiration:

   > Oh and we need to be able to stage and commit and push obviously.

3. Changes variant a, and the agent fills the commit (2026-11-12):

   > I like variant a, just make sure we can use the agent to fill in the
   > commit (subject and message)

4. Standing rules carried in: Evidence is cut (RFC-BPR-0109 Deferred); the
   agent choice lives in the status-bar footer; no grey explainer text
   (tooltips instead); nothing moves on click; consistent button sizes; calm
   native macOS.

## Frames

| Frame | File | Renders |
|---|---|---|
| A: a local draft in the pull request window (Review, Overview, publish and candidate states) | [option-a-twin.html](option-a-twin.html) | `option-a-twin-a1-review-*`, `-a2-overview-*`, `-a3-states-*` |
| Home variant 1: one 26 px line per draft in the Open row | [home-entry.html](home-entry.html) (variant 1 only is accepted) | `home-entry-h1-open-row-*` |
| Author state: the same window on your own worktree | [author-state.html](author-state.html) | `author-state-s1-*`, `-s2-*` |
| The view picker and the Edit view | [view-and-edit.html](view-and-edit.html) | `view-and-edit-p1-*`, `-p2-*` |
| Changes variant a: one comparison at a time, stage, commit, push | [changes-a-scope.html](changes-a-scope.html) | `changes-a-scope-ca1-*`, `-ca2-*` |

Not accepted and not copied: options B and C, Home variant 2 and Changes
variant b (they stay in the session scratchpad where they were drawn).

## What the accepted design says

- A local draft opens in the pull request window. Overview holds the title,
  description and files; Review holds the diff and the accepted Tour panel.
  The candidate is one pop-up in the Comparing slot; its menu lists the
  captured versions and holds Refresh and Discard…. **Create Draft PR…** sits
  where Submit review sits. A Status drawer tab carries the publish steps
  (push, create the draft pull request, on GitHub). When the pull request is
  created the same window becomes the pull request.
- Home: no band. Each local draft is one 26 px row in the Open row's grid,
  first on the page, saying Local draft. The group's accessible name is In
  preparation.
- Author state is a state of the pull request window, not a mode: the window
  is on your own worktree, for a local draft or for a pull request opened by
  its author. Its signals: the branch beside the title with ahead and behind
  counts; the view picker's third icon; the Local Changes drawer tab; the
  status bar's counts. Nothing is recoloured or banded.
- The view picker is a segmented pick in the review bar: two icons (two
  panes, one pane) in a review, three in author state (Edit: no diff, the
  worktree's file, editable). The slot is three icons wide either way.
- Edit shows the worktree file in the existing code editor with a dot, Not
  saved and Save in the file header, and gutter marks for added, changed and
  removed lines (dashed until saved). A file changed on disk offers Reload in
  Save's place and writes nothing.
- Three comparisons with one right-hand side (your files on disk):
  Uncommitted (left: your last commit), Not pushed (left: the pull request's
  head on GitHub, or the branch on GitHub before a pull request) and Whole PR
  (left: the target at the merge base).
- Local Changes, variant a: a three-way pick (Uncommitted, Not pushed, Whole
  PR) with counts. Uncommitted shows Unstaged over Staged, each a folder tree
  with a status letter per file and one header button (Stage, Unstage). In
  the diff, each change has Stage Change or Unstage Change. A commit box is
  pinned under the lists: subject, description, Amend (only while the last
  commit is not on GitHub), and one counting button, Commit N Files. Under it
  the push line: where the branch stands (ahead, up to date, behind), Push,
  and the exact command. Never force; a refused push turns the line red,
  says what GitHub has that the branch does not and offers Check Again.
- The agent fills the commit subject and description from the staged change
  (owner's choice 3).

## Decisions taken here

- **The drawer tab is named Local Changes.** The review bar already says
  "Changes 2 of 3" for the changes within a file; a tab also called Changes
  would read as the same thing. Local Changes is Fork's name, which the owner
  pointed to, and says what the tab holds. Decided 2026-11-12 by the
  implementer under the brief's instruction to decide and record.
- **The agent's commit message** is a one-shot, fresh, tool-free run with the
  status-bar footer's agent and its own editable prompt purpose,
  `local.commit-message`, which RFC-BPR-0107's sharing format recognises. Its
  input is the staged diff and the recent commit subjects; its result fills
  the subject and description as an editable suggestion and never commits.
  Its button carries the Agent accent.
- Local Changes, Edit and partial (per-change) staging are pulled forward
  from the local-draft-authoring pack (RFC-BPR-0113, RFC-BPR-0114) into this
  lane, amended as recorded in those RFCs and in
  [the pack's DECISIONS.md](../../rfcs/assets/local-draft-authoring-v1/DECISIONS.md).

## Where the drawings depart from an RFC

Recorded with each RFC's owner amendment:

- A opens on Review; RFC-BPR-0108 said a draft opens on Changes.
- Home variant 1 drops the visible In preparation title RFC-BPR-0108 AC06
  called prominent (it is the group's accessible name).
- Publish the tour is drawn as a waiting third step; RFC-BPR-0111 records
  that follow-up as not built.
- Discard… is in the candidate's menu.
- RFC-BPR-0113 offered Edit in the draft's Changes area and no Edit in a pull
  request review; here Edit is a view of the picker, available in author
  state, including on a pull request opened by its author.
- RFC-BPR-0114 said "There is no amend, no staging view, no commit of part of
  a file" and committed ticked files through a Commit… sheet; this design has
  staging lists, Stage Change and Amend, and the push command always on show
  instead of a preview sheet. Its one-time hooks question stands.
- RFC-BPR-0122's Feedback area is not drawn.

## Open points the drawings left

- Whether the candidate chooser survives once commits are made in the window
  (a candidate and a commit say nearly the same thing).
- Staging single lines (the Stage Change tooltip promises it; not drawn).
