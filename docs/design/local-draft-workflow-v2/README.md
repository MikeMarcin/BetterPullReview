# Revise: a mode for the author, with the checklist first (round 2)

Mockups for the owner to accept or reject. No product code changed.

[Round 1](../local-draft-workflow-v1/README.md) drew four places for the
author's checklist. The owner chose **B, Workflow tab** on 2026-11-15 and
said, in the look's note and afterwards:

> I think I like workflow, it should become the first tab and become active
> when we enter that mode. It's very close to A but doesn't confuse the user
> by making status have different meanings across surfaces.

> Sent back is too strong, maybe that happens, maybe i just decide to make a
> change, maybe there's just a light optional nitpick to handle.
>
> Publish update is better than Publish response

> So on an open pull request I own, I get to this mode how? Is there a
> button to switch between it and the normal review bench?

> I think a Revise/Review toggle is probably my preference

> Oh you know what. Maybe revise becomes a modality at the top that only
> shows up when you own the PR

This round reads the last remark as: **Revise is a fifth mode in the
window's mode switcher**, beside Overview, Review, Discuss and Threads (the
application's own name for them is modality: `reviewModalityOverview`). It
replaces the toggle of the remark before it. If the owner meant something
else, the round is wrong at its root and says so in its question.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames in both themes; none
is a screen capture. Content is invented. The `frontend-design` skill was
loaded for this work.

## The design

- **Where it sits.** Revise is the last mode in the switcher and takes ⌘5.
  Overview, Review, Discuss and Threads keep ⌘1 to ⌘4. It is there only when
  the signed-in account opened the pull request, and for a local draft. On
  anyone else's pull request the switcher is today's four modes.
  [dark](png/switcher-dark.png), [light](png/switcher-light.png).
- **What it holds.** The author's bench that was
  [accepted on 2026-11-12](../local-draft-pr-window-v1/README.md), moved out
  of Review into a mode of its own. Left: **This update**, the files that
  would go (Uncommitted, Not pushed, Whole PR) with the commit box, and under
  them the open conversations. Middle: the selected file. Right: the drawer,
  which opens on **Workflow**.
- **Workflow is the first drawer tab** and holds the author's checklist:
  round 1's list, with the workflow's actions as rows in it.
- **The handoff** is one button at the right end of Revise's bar.
- **Status keeps its one meaning.** It is Overview's tab and says what GitHub
  says, for an author as for anyone. Nothing about the workflow is added to
  it. [dark](png/status-dark.png), [light](png/status-light.png).

The marks are round 1's: filled is something BetterPullReview saw itself,
outlined is something a companion reported.
[dark](png/model-dark.png), [light](png/model-light.png).

## The three cases

| | A local draft | A draft pull request I own | Revising my pull request |
|---|---|---|---|
| Handoff | Create Draft PR…, or Hand Off Plan… when the org tool is the publisher | Ready for review | Publish update… |
| Publisher | Chosen in the list | Settled; the list says who made the pull request | Settled |
| Left column | The files, the commit box | The files; conversations when there are any | The files, the open conversations |
| Ready means | The branch on GitHub is this candidate and every required check is reported passed on it | Nothing unpushed, GitHub's checks passed, every required check reported passed on the pushed head | My changes are committed, the replies I mean to send are drafted and every required check is reported passed on the new candidate |
| Window | [dark](png/window-local-dark.png), [light](png/window-local-light.png) | [dark](png/window-draft-dark.png), [light](png/window-draft-light.png) | [dark](png/window-revise-dark.png), [light](png/window-revise-light.png) |
| Workflow tab, four states | [dark](png/states-local-dark.png), [light](png/states-local-light.png) | [dark](png/states-draft-dark.png), [light](png/states-draft-light.png) | [dark](png/states-revise-dark.png), [light](png/states-revise-light.png) |

The four states are no workflow, nothing run yet, some reported or failed,
and ready.

## The wording

- The third case is **Revising my pull request**. It does not say why: its
  four states are drawn as a change of my own before any review, an optional
  nit, a review that asked for changes, and ready.
- The handoff is **Publish update…**: the push, then any replies drafted.
- The headline of an open pull request is its number and what is left, not
  what a reviewer did. A reviewer's verdict is one fact row under On GitHub,
  in GitHub's own words.
- The group that was Feedback is **This update**, with Changes and
  Conversations. Its buttons are Show and Go to them.
- "Sent back" and "Publish response" are gone from every label.

## Recommended defaults

| Question | Default |
|---|---|
| Position and key | Last in the switcher, ⌘5. Nothing renumbers, and nothing is missing from the middle when the mode is absent. ⌘5 is not bound today |
| What opens in Revise | A local draft and a draft pull request I own. An open pull request I own opens on Overview, as today, with Revise one key away. After that a window reopens in the mode it was left in |
| What Revise remembers for each pull request | The comparison picked, the file and the drawer tab. The first entry opens the drawer on Workflow |
| Without a worktree | Revise is still in the switcher. Its page asks for the worktree once, and the rows that need one wait for it |
| The handoff | At the right end of Revise's bar. Ready for review also stays on the title's line in Overview, where it is today (BPR-1852) |
| A local draft's Status tab | Its publish steps move into Workflow, so a local draft has no Status tab until the pull request exists |
| Review | The Edit view and the Local Changes tab leave Review for Revise. Review is for reading again |

## What the application lacks today

- A workflow belongs to a local draft. A pull request that never had one
  needs a worktree associated with it
  ([RFC-BPR-0122](../../rfcs/RFC-BPR-0122-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md),
  Proposed).
- Publish update… is the Publish response… of
  [RFC-BPR-0124](../../rfcs/RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md)
  under the owner's name for it, and is Proposed. So are the conversation
  items of RFC-BPR-0122 and the agent's replies of
  [RFC-BPR-0123](../../rfcs/RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md).
- The switcher has four fixed modes. A fifth that comes and goes is new.
- The accepted author's bench put Local Changes and the Edit view in Review.
  This round moves them.

## Left out of this round

- Revise without a worktree, and the worktree chooser.
- The Comments, Notes and Agent tabs of Revise's drawer.
- The workflow's rarer states, as in round 1.

## Renders

As round 1's: Chrome's headless mode on a throwaway profile, one frame and
theme at a time (`?theme=dark&frame=window-revise`), trimmed to the frame,
kept at 1.5 times and reduced to 256 colours. `frame` is `model`, `switcher`,
`status`, or `window-` or `states-` joined to `local`, `draft` or `revise`.

## Status

Filed for the owner on 2026-11-15 as attended review
`local-draft-workflow-mockups-v002`: one Accept or Reject question on this
design. External state directory
`~/.attended-review-state/local-draft-workflow-mockups-v002-2026-11-15/`
(`state/` is the session and holds its ID). Lifecycle: awaiting the owner's
answer. Nothing is built until it is given.
