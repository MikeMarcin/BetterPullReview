# The author's checklist: four places it could live (round 1)

Mockups for the owner to choose from. No product code changed.

The Workflow area of
[RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md) took
over a local draft's Status drawer: two truncated pickers, then a paragraph
for every action. The owner rejected it on 2026-11-15:

> What the heck happend to my status drawer?

and then widened the question:

> I mean if we don't need status tab then we don't need it here. If there's
> information that isn't relevant we don't need to keep it. This looks to be
> the PR Authoring workflow so make sure whatever we design works for a Local
> PR, an existing Draft PR that we own, and a live PR we need to make
> revisions to.

So this round does not restore the drawer. It asks where an author's
checklist lives, and draws four answers in three cases:

- a local draft, before anything is on GitHub;
- a draft pull request the author owns;
- the author's open pull request, sent back by a reviewer.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames in both themes; none
is a screen capture. Content is invented. The workflow is the repository's
synthetic organization fixture (`tests/fixtures/workflow_companion`).
The `frontend-design` skill was loaded for this work.

## What the four options share

- **One checklist, three handoffs.** The list answers "what is left before I
  hand this over?" The handoff is Create Draft PR… for a local draft, Ready
  for review for a draft pull request and Publish response… for a pull
  request sent back. It is one button at the right end of the review bar,
  where Submit review sits for a reviewer.
- **A workflow adds rows, not a panel.** Its actions are rows among the
  author's other steps. Without a workflow the list is shorter and one quiet
  control offers the workflow.
- **Who runs a row is said once.** An agent action runs in BetterPullReview
  and wears the agent accent. A companion action is one the author runs in
  their own terminal: Hand off shows the exact command with Copy. Each kind
  has one glyph and one tooltip.
- **Trust is in the mark.** A filled mark means BetterPullReview saw it
  itself, on this computer or on GitHub. An outlined mark means a companion
  reported it, and the words say Reported. Nothing a companion returns is
  drawn filled. Legend: [dark](png/model-dark.png),
  [light](png/model-light.png).

Every option still says what RFC-BPR-0112 requires: the workflow and its
revision; the publisher; each action's name, kind and state; the required
checks; reported, never observed; the agent accent on agent actions only.

### What is removed, and why

| Removed | Why |
|---|---|
| What is sent (From, Into, As, Version, Kept here) | The Create Draft PR… sheet shows it before anything is sent |
| My review (files read, comments in review) | It is a reviewer's; the author does not review their own pull request |
| The People faces | One Reviewers row says who is asked and offers the request |
| The Workflow and Publisher pickers | The workflow is one quiet line; the publisher is two radio buttons |
| The paragraph under every action | The run model is two tooltips and the trust rule is the mark |

### What differs by case

| | A local draft | A draft pull request I own | My open pull request, sent back |
|---|---|---|---|
| Handoff | Create Draft PR…, or Hand Off Plan… when the org tool is the publisher | Ready for review | Publish response… |
| Publisher | Chosen here: BetterPullReview or the org tool | Settled; the list says who made the pull request | Settled |
| Workflow actions | All; the follow-up waits for the pull request | All but publishing, again on each new candidate | All but publishing, again on each new candidate |
| Other rows | Push, create, publish the tour | Push, GitHub's checks, reviewers | Conversations, what changed since the review, push, GitHub's checks, reviewers |
| Ready means | The branch on GitHub is this candidate and every required check is reported passed on it | Nothing unpushed, GitHub's checks passed, every required check reported passed on the pushed head | Every conversation answered, the fixes committed, every required check reported passed on the new candidate |

## The options

Each option has one window (a local draft with a required check reported not
met and the org tool chosen as publisher) and one board per case. A board
shows four states: no workflow, nothing run yet, some reported or failed, and
ready.

| | Where the checklist is | Status tab | In view from | A command to copy | Without a workflow |
|---|---|---|---|---|---|
| A, One list | The drawer's Status tab | Kept, as the checklist | Overview | Under its row, on one line | The publish steps and one link |
| B, Workflow tab | A second drawer tab, for the workflow's rows only | Kept, as today | Overview | Under its row, on one line | An empty tab with one button |
| C, Overview table | A table over Files | Removed | Overview | In its row, on one line | A short table and one link |
| D, Header popover | A popover from the review bar | Removed | Every tab | Under its row; the popover closes when the author leaves | A few words beside the button |

### A, One list: the Status tab becomes the author's checklist

The drawer keeps one tab and it holds one list: a headline that says where
the pull request stands, then every step in small groups. The workflow's
actions are rows in Prepare. Its name and revision are a quiet line at the
foot, which is also where a draft without one offers it.

- Window: [dark](png/a-window-dark.png), [light](png/a-window-light.png).
- Local draft: [dark](png/a-local-dark.png), [light](png/a-local-light.png).
- Draft pull request: [dark](png/a-draft-dark.png),
  [light](png/a-draft-light.png).
- Sent back: [dark](png/a-live-dark.png), [light](png/a-live-light.png).
- Gives: one place, always in view on Overview, that reads top to bottom as
  what is left; the publisher choice sits on the publish steps it changes;
  the same list grows from a local draft to a pull request under review;
  the publish steps stay where they were.
- Costs: the drawer is 340 points wide, so an action takes two lines; with a
  workflow and feedback the list reaches about 560 points and scrolls in a
  short window; showing a command moves the rows under it; the Status tab is
  on Overview only.

### B, Workflow tab: Status as it is, and a second tab for the workflow

The smallest change. Status stays what the application shows today, less My
review and What is sent, and the workflow gets its own tab. The tab's label
counts what the workflow still needs, and Status gains one line that opens
it.

- Window: [dark](png/b-window-dark.png), [light](png/b-window-light.png).
- Local draft: [dark](png/b-local-dark.png), [light](png/b-local-light.png).
- Draft pull request: [dark](png/b-draft-dark.png),
  [light](png/b-draft-light.png).
- Sent back: [dark](png/b-live-dark.png), [light](png/b-live-light.png).
- Gives: Status is untouched by a workflow; the workflow has a whole tab;
  least to build.
- Costs: two places to look, split by where a fact comes from and not by
  what to do next; the publisher is chosen in one tab and changes the steps
  in the other; a draft without a workflow still carries the tab, or the tab
  comes and goes; Status keeps sections made for a reviewer, and the
  feedback rows of a pull request sent back have no home in it.

### C, Overview table: the checklist is a table over Files, and the Status tab goes

The checklist is a table in Overview's main column, in the Files table's
manner: Step, Run by, State and one button. The author's Overview has no
Status tab. Run by says who acts in a word, so no glyph has to be learned.

- Window: [dark](png/c-window-dark.png), [light](png/c-window-light.png).
- Local draft: [dark](png/c-local-dark.png), [light](png/c-local-light.png).
- Draft pull request: [dark](png/c-draft-dark.png),
  [light](png/c-draft-light.png).
- Sent back: [dark](png/c-live-dark.png), [light](png/c-live-light.png).
- Gives: the most room, one line per step and who runs it in plain words; no
  Status tab and nothing kept that is not about authoring; a table the
  application already has.
- Costs: it pushes Files down by its own height, about 200 points for a
  local draft with a workflow and about 330 for a pull request sent back;
  Overview only; the description and the list compete for the top of the
  page; showing a command grows its row.

### D, Header popover: the handoff's status is a button

The status beside the handoff button becomes a button. It says what is left
in a few words and opens the checklist as a popover. There is no Status tab,
and the status is in view on every tab.

- Window: [dark](png/d-window-dark.png), [light](png/d-window-light.png).
- Local draft: [dark](png/d-local-dark.png), [light](png/d-local-light.png).
- Draft pull request: [dark](png/d-draft-dark.png),
  [light](png/d-draft-light.png).
- Sent back: [dark](png/d-live-dark.png), [light](png/d-live-light.png).
- Gives: in view from Overview, Review, Discuss and Threads; nothing in the
  window moves, and Overview's width goes to the description and Files; the
  status and the handoff are side by side.
- Costs: closed, it shows a few words, and which step is waiting takes a
  press; a handoff means leaving for a terminal, and a popover closes when
  the application loses focus, so the author reopens it to read the answer;
  a tall popover covers the code it is about; Ready for review moves from
  the title's line to the bar.

## Recommendation

**A, One list**, keeping the status words beside the handoff button that the
[accepted local-draft design](../local-draft-pr-window-v1/README.md) already
has.

It answers both of the owner's remarks. The drawer shows status again, and
only an author's status: the publish steps stay and the reviewer's sections
go. It is the only option where the publisher sits on the steps it changes,
and one list serves all three cases, growing a group at a time. Its cost is
width. If the two-line rows read as cramped, C is the same list with room, at
the price of pushing Files down.

## What the application lacks today

Each option needs these, whichever is chosen:

- **A workflow belongs to a local draft.** A pull request that never had one
  has no workflow rows until a worktree is associated with it, which
  [RFC-BPR-0122](../../rfcs/RFC-BPR-0122-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md)
  proposes and nothing builds.
- **The feedback rows are proposed, not built.** Address feedback
  (RFC-BPR-0122), the agent's evaluation and replies
  ([RFC-BPR-0123](../../rfcs/RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md))
  and Publish response… with its re-request
  ([RFC-BPR-0124](../../rfcs/RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md))
  are Proposed. Today the author has Threads, reply drafts, Resolve and
  Request review.
- **"Since a reviewer's review" is the reviewer's question today.**
  [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md)
  answers it for the person who reviewed. The author's row needs the same
  comparison from the other side.
- **The author sees a reviewer's drawer.** Overview shows My review to the
  person who opened the pull request
  (`src/adapters/qt/src/pull_request_overview_page.cpp`).
- **Ready for review ends the title's line** (BPR-1852). Every option moves
  or repeats it at the right end of the review bar.
- **The Status tab is on Overview only.** A, B and C are out of view in
  Review; D is not.

## Tooltips

The suite requires 60 characters or fewer, no semicolon and no line break.
Every tooltip drawn meets that; `index.html` carries them as `title`
attributes.

| On | Tooltip |
|---|---|
| An outlined mark | Reported by the companion, not observed by BetterPullReview |
| A filled mark | BetterPullReview saw this itself |
| The terminal glyph | Runs in your terminal, where you approve what it does |
| The agent glyph | Writer runs this in BetterPullReview and can only propose |
| Required | This workflow needs it before its publisher gets the plan |
| The BetterPullReview publisher | Does not run this workflow's checks |
| The org tool publisher | You run the org tool in your terminal with the exact plan |
| A handed-off mark | Handed off. The answer comes from your terminal |
| A stale mark | It ran on an earlier candidate |
| The workflow's line | From Synthetic Org, revision 363f805d21d0 |
| Use Synthetic Org pull request | Optional. Selecting it runs nothing |

## Left out of this round

- The workflow's rarer states: an input request with its answer field, an
  uncertain publication with Check GitHub, a newer revision of the workflow
  to accept, and a missing program or agent.
- A profile with several workflows. The quiet line is a pop-up button, so it
  would list them; only one is drawn.
- The Review tab. Every window is Overview.

## Renders

Chrome's headless mode on a throwaway profile, one frame and theme at a time
(`?theme=dark&frame=a-local`), trimmed to the frame, kept at 1.5 times and
reduced to 256 colours. `frame` is `model`, or a letter `a` to `d` joined to
`window`, `local`, `draft` or `live`.

## Status

Filed for the owner on 2026-11-15 as attended review
`local-draft-workflow-mockups-v001`: one question, a choice between A, B, C
and D. External state directory
`~/.attended-review-state/local-draft-workflow-mockups-v001-2026-11-15/`
(`state/` is the session and holds its ID). Lifecycle: awaiting the owner's
choice. Nothing is built until it is made.
