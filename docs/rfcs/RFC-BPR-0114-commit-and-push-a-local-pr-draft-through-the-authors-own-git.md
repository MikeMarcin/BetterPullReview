---
id: "RFC-BPR-0114"
title: "Commit and push a local PR draft through the author's own Git"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the local-draft-authoring-v1 pack, where it was RFC-DRAFT-0002","inspected_revision":"5380330fb1e1f15f5a3da476e0e8260ba4a730a3","milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"baseline":"b0d54697a6db6b17b459a751515de60aff14d65d","reopened_choices":["PUBLISH-01"]}
depends_on: ["RFC-BPR-0108", "RFC-BPR-0111"]
---

# Commit and push a local PR draft through the author's own Git

## Problem and grounded current behavior

[RFC-BPR-0111](RFC-BPR-0111-draft-publication-and-continuity.md)
settled PUBLISH-01 on 2026-11-10: the author commits and pushes outside
BetterPullReview, and BPR creates the draft pull request from the pushed
branch. When the hosted branch is missing or differs, "preparation names the
exact push to run and observes the remote again rather than pushing", and
when a candidate holds uncommitted files it "hands the specific remaining
task to the author's editor/harness".

The deciding constraint was the Git BPR runs. It is isolated on purpose: "an
owner-private isolation root with its own home, configuration, empty
templates and hooks, no system configuration, no credential helper, no
protocol and no lazy fetch"
([installed_git_publication.hpp](../../src/adapters/git/include/bpr/adapters/git/installed_git_publication.hpp)).
A commit made that way would skip the author's identity, signing and hooks,
and a push cannot be made at all.

The same day the owner asked that the local draft "allow us to edit the
files and commit them / push them" ([note 1](assets/local-draft-authoring-v1/SOURCE_NOTES.md)), and, asked
"Reopen PUBLISH-01 so BPR runs commit and push through your own Git?",
answered "Yes, reopen it" ([note 4](assets/local-draft-authoring-v1/SOURCE_NOTES.md)). A plain commit and a
plain push are mechanical; under
[RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md) they are
ordinary controls, not agent ones, and a local draft must stay complete
without an agent.

## Proposed behavior and boundaries

### PUBLISH-01, reopened

PUBLISH-01 is settled again, differently: on the author's press, BPR itself
commits the selected files and pushes the draft's branch, by running the
author's own Git. BPR still creates the pull request from the verified hosted
head through the existing REST authority, and still publishes the author's
real commits unchanged. Naming the exact commit or push for the author to
run, as RFC-BPR-0111 has it, remains the path wherever the author declines
the question below or their own Git cannot be run. The
[proposed amendment](assets/local-draft-authoring-v1/AMENDMENTS.md) gives the before and after text.

### The author's own Git

A second Git runner, separate from the isolated one, runs the Git executable
BPR already resolved in the local draft's worktree with the author's
environment: their home and their global and system configuration, the
repository's configuration and hooks, their credential helpers and their
signing setup. BPR reads, stores and passes no credential; Git's own helpers
handle them.

- It runs only the operations this RFC names, only in a local draft's own
  worktree, and only from an explicit press. Opening, reading, refreshing
  and previewing run nothing through it.
- The isolated runner, its configuration and everything that uses it are
  unchanged.
- It has no terminal. Git's terminal prompt is off, so an operation that
  needs typed input fails with that reason instead of waiting. A graphical
  helper the author has configured (a credential manager, a passphrase
  dialog) may show its own window.
- Its environment is the application's, not a login shell's. A hook that
  needs a tool the application's environment lacks fails as it would for
  any graphical Git client, and the failure names the command to run in a
  terminal.
- Each operation states what it is waiting for and has a bounded wait, as
  [RFC-BPR-0105](RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md)
  requires, and can be cancelled. After a cancellation, a timeout or a lost
  answer BPR observes what happened and never repeats the operation blindly.
- No other work waits on it: a review submission in another window completes
  while a push is in flight.

The first Commit or Push in a local draft asks once: "Commit and Push run
this repository's Git hooks and your Git configuration on this computer,
with your account's rights. Allow for this local draft?" The answer is
remembered for that local draft. Declining runs nothing and shows the exact
commands to run instead.

### Commit

**Commit…** is offered when the candidate holds selected uncommitted files.
It shows those files, the branch, the name and email Git will record, and a
message field that must not be empty.

- It makes one commit on the draft's checked-out source branch holding
  exactly the selected files' captured content. Every other change in the
  worktree, staged or not, is left as it was: neither committed nor
  unstaged.
- It runs only when the checked-out branch is the draft's source branch, its
  head is the candidate's head, and each selected file on disk holds its
  captured bytes. Otherwise it runs no Git and offers the refresh.
- The repository's commit hooks and the author's signing configuration apply.
- A hook that refuses leaves no commit; BPR shows the hook's and Git's
  output and the command to run in a terminal.
- After the commit BPR observes the new head. When its content equals the
  candidate's, the candidate is recorded as committed at that head and its
  notes, evidence and tour stay attached. When a hook changed the content,
  the commit stands, BPR says the committed content differs from the
  candidate, and the refresh is offered.

There is no amend, no staging view, no commit of part of a file, and no
branch creation or switching.

### Push

**Push** is offered when the candidate is fully committed and the hosted
branch is missing or behind the draft's branch. Its preview names the
branch, the hosted repository and URL, the commits that will be sent, and
whether the branch will be created.

- It sends the one branch to the hosted repository RFC-BPR-0108 resolved,
  using the URL as the clone that names that repository has it configured.
  In an isolated local clone it never pushes to `origin`, which is the
  author's primary clone.
- It creates the branch or fast-forwards it. It never forces, never deletes
  and sends no tag.
- It writes no configuration and sets no upstream, and it moves no branch in
  the clone.
- Pre-push hooks run from the worktree that has the branch checked out.
- A refused push (not a fast-forward, a protected branch, a hook, failed
  authentication) shows Git's output and its reason. For a push that is not
  a fast-forward BPR says the hosted branch holds commits the local branch
  does not, or that history was rewritten, and offers no way to force.
- With a signed-in account BPR then observes the hosted head through the
  existing read path and continues as RFC-BPR-0111 defines: when it equals
  the candidate, **Create draft PR…** is ready without leaving the
  application. Signed out, the push can still succeed with Git's own
  credentials; BPR reports Git's result as reported, not observed, and
  creating the pull request still needs the sign-in.
- From a local draft already associated with a pull request, Push sends
  later commits to that pull request's branch by the same press. Nothing
  pushes automatically, and the pull request's new head arrives through the
  existing synchronization.

Commit, Push and their failures report as
[RFC-BPR-0099](RFC-BPR-0099-tour-publishing-in-reach-and-reported.md)
defines for tour publication: steps as waiting, running, done, failed or not
run, and a failure as an error-role banner naming what stopped, why, what
now exists and what moves forward. Output shown from Git or a hook is
bounded, and the implementation records the bound.

### What this does not become

Local PR preparation is still not a general Git client. BPR's own-Git
operations are exactly two: commit the selected files, and push the draft's
branch. Merge, squash and rebase exist only as the agent runs of
[the change-run RFC](RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md).
Force push, branch deletion, tags, stash, reset and checkout exist nowhere.
Pull request creation keeps its host qualification from RFC-BPR-0111; a push
works wherever the author's Git does, which does not qualify native creation
on that host.

The runner is qualified per platform. Where it is not yet qualified, Commit…
and Push are absent and the named commands remain.

### Architecture

Core decides when Commit and Push are offered, their preconditions, the exact
operation and its refusal and outcome types. The application owns the
author-Git port, its lifecycle, cancellation and the observations that
follow. The port lives with the Git adapter, shares process supervision with
the existing runners, and shares no state, store or configuration with the
isolated one. Qt presents. `architecture_boundaries` holds the operation
catalogue to the two operations above.

Proposals in this RFC that the owner has not separately confirmed are listed
in the [decision record](assets/local-draft-authoring-v1/DECISIONS.md#decisions) as P01, P02 and P08.

### Owner amendment, 2026-11-12: Local Changes, staging, Amend and the agent's message

The owner, choosing the pull request window for local drafts and its author
state ([accepted design](../design/local-draft-pr-window-v1/README.md)):

> And then we probably need a better way to see changes. There are a few kinds
> that matter. Changes relative to merge base, changes relative to PR head,
> and local changes not yet committed even locally.

> Oh and we need to be able to stage and commit and push obviously.

> I like variant a, just make sure we can use the agent to fill in the commit
> (subject and message)

Changed requirements; everything not named here stands, including the
author-Git runner, the one-time hooks-and-configuration question, bounded and
cancellable operations, never forcing, and the observation after each
operation.

- **Local Changes.** A drawer tab in author state (named Local Changes, not
  Changes, because the review bar already says "Changes 2 of 3"). A three-way
  pick heads it: **Uncommitted** (left side: the last commit), **Not pushed**
  (left side: the pull request's head on GitHub; before a pull request, the
  branch on GitHub; before that, unavailable) and **Whole PR** (left side:
  the target at the merge base). The right side is always the files on disk.
  The pick is the review bar's Comparing pop-up in a second place.
- **Staging.** Uncommitted lists Unstaged over Staged, each a folder tree with
  a status letter per file and one header button, Stage or Unstage, acting on
  the selected file (a double-click does the same). In the diff, each change
  of an Uncommitted comparison has Stage Change or Unstage Change. A file with
  some changes staged is in both lists. "There is no amend, no staging view,
  no commit of part of a file" is withdrawn: partial commits are made by
  staging changes. Staging writes only the index; staging a change writes the
  file's index entry with exactly that change applied to the staged text.
- **Commit.** A box pinned under the lists: subject, description, Amend, and
  one counting button, **Commit N Files**, enabled when something is staged,
  the subject is not empty and nothing is unmerged. It commits the index
  (what is staged, and only that) through the author's own Git, with their
  identity, signing and hooks. The Commit… sheet over ticked candidate files
  is replaced by staging; "every other change in the worktree, staged or not,
  is left as it was" becomes "every unstaged change is left as it was".
  Commit's preconditions are on the branch (checked out, not detached, the
  draft's branch) rather than on a candidate.
- **Amend** is offered only while the last commit is not on GitHub (not
  reachable from the hosted branch or the pull request's head). After that it
  dims and its tooltip says amending would need a forced push.
- **Push.** One line under the commit box says where the branch stands against
  the pull request or hosted branch (ahead, up to date, behind), with Push and
  the exact command as it will run, always on show; the preview sheet is
  replaced by that line and the Not pushed list. When the hosted branch has
  commits the local branch does not, Push is refused before anything runs:
  the line turns red, says what GitHub has that the branch does not and that
  nothing was sent, and offers Check Again. No control forces.
- **The agent fills the commit.** A control beside the subject asks the agent
  chosen in the status-bar footer for a subject and description. It is a
  one-shot, fresh (never resumed), tool-free run whose input is the staged
  diff and the branch's recent commit subjects, under its own editable prompt
  purpose `local.commit-message` (Settings, beside the local-authoring
  prompts), which [RFC-BPR-0107](RFC-BPR-0107-agent-and-prompt-sharing.md)'s
  sharing format recognises. Its answer fills the two fields as an editable
  suggestion and never commits. The control carries the Agent accent, and the
  checkout guards of [RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)
  apply: the run gets no tools and no worktree.
- **Where.** Author state includes a pull request opened by its author on
  their worktree (the same window [RFC-BPR-0122](RFC-BPR-0122-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md)
  opens for Address feedback), not only a local draft.

- **Reads.** Local Changes reads the worktree through the author's own Git,
  read-only (status, the changed paths against a commit, blob bytes, the
  staged diff), with no optional locks and no fsmonitor. Such a read runs no
  hook, but Git may run a repository's configured clean filter as any Git
  client's status does. Opening the window reads only the worktree: GitHub is
  read (`ls-remote`, through the author's credentials) only when the author
  presses Check Again or Push, so ahead and behind say "Not checked with
  GitHub" until then. (Implementation decision, 2026-11-12, keeping P01's
  "only on a press" for anything that leaves the computer.)

The operation catalogue becomes: stage a path, unstage a path, stage one
change, unstage one change, commit (with or without amend), and push the
draft's branch. Force, delete, tag, reset of the worktree, stash and checkout
still exist nowhere.

AC01 reads: Commit N Files makes one commit on the checked-out branch holding
exactly the staged content, recorded with the author's Git identity and
signing configuration after the repository's commit hooks ran, and every
unstaged change is as it was before; Stage Change stages exactly that change.
AC02 reads: Commit runs no Git when nothing is staged, the subject is empty,
the head is detached or a path is unmerged; a refusing hook leaves no commit
and its output is shown; Amend is refused while the last commit is on GitHub.
AC08 reads: on a frozen build, the owner accepts Local Changes with staging,
the commit box with the agent's message, the push line and its refused state,
and the one-time question.

Delivery: pulled forward from the local-draft-authoring plan into
`v0-5-0-prerelease-targets-v1` on lane
`claude/v0-5-0-prerelease-targets-52265d` (BPR-2065, BPR-2066, BPR-2067).

## Acceptance criteria

- **AC01:** With selected uncommitted files, Commit… makes one commit on the draft's branch holding exactly those files' captured content, recorded with the author's Git identity and signing configuration and after the repository's commit hooks ran, and every other staged or unstaged change is as it was before.
- **AC02:** Commit… runs no Git when the branch, head or a selected file differs from the candidate. A refusing hook leaves no commit and its output is shown. A hook that changes content leaves a commit BPR reports as differing from the candidate, with the refresh offered.
- **AC03:** Push sends the draft's branch to the resolved hosted repository, including from an isolated local clone whose `origin` is the primary clone, runs the pre-push hooks from the worktree, authenticates through the author's own credential helper, and leaves the clone's configuration and branches unchanged; BPR then observes the hosted head and Create draft PR… proceeds without the author leaving the application.
- **AC04:** A push that is not a fast-forward is refused and reported with its reason, and BPR's own-Git operation catalogue holds no force, delete, tag, reset, stash or checkout operation.
- **AC05:** The first Commit or Push in a local draft asks the hooks-and-configuration question once. Declining runs nothing and shows the exact commands. The isolated runner still runs with no hooks, credential helper, protocol or system configuration.
- **AC06:** An operation that needs typed input fails within its bounded wait with that reason. After a cancellation or a lost answer BPR observes the result and repeats nothing. A review submission elsewhere completes while a push is in flight.
- **AC07:** From a local draft associated with a pull request, later commits reach the pull request's branch only on the author's Push.
- **AC08:** On a frozen build, the owner accepts the Commit… and Push previews, the one-time question and a failed-hook banner.

## Proposed delivery

[The author-Git runner and Commit](../issues/20-ready/BPR-2065-author-git-runner-and-commit.md),
[Push and its meeting with Create draft PR](../issues/10-backlog/BPR-2066-push-and-create-draft-pr-in-one-flow.md)
and [closeout](../issues/10-backlog/BPR-2067-close-rfc-0114.md) form the second
[milestone](plans/local-draft-authoring-v1/milestone-plan.json). Push needs the native pull request
creation of RFC-BPR-0111 to exist first.
