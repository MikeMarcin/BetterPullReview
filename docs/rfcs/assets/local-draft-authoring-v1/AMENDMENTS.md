# Proposed amendments to adopted RFCs

This pack changes requirements in three adopted RFCs and clarifies two more.
All five are `Proposed`, adopted under mandate
`v0-5-0-prerelease-targets-r1` on lane
`claude/v0-5-0-prerelease-targets-52265d`, and were read at
`5380330fb1e1f15f5a3da476e0e8260ba4a730a3`. Their cards are all Backlog: no
work is claimed, and no acceptance evidence exists that this could
invalidate.

Authoring leaves the live text unchanged. Each amendment is applied by the
milestone of this pack that owns it, through `implement-rfc`. The adopted
plan `v0-5-0-prerelease-targets-v1` is not replayed or re-adopted.

## A1. RFC-BPR-0111: PUBLISH-01 reopened

Source: [RFC-BPR-0111](../../RFC-BPR-0111-draft-publication-and-continuity.md),
"Create draft PR is a deliberate boundary", "Native branch decision" and
AC01; decision D13 in
[the earlier pack's decision record](../v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).

Rationale: the owner's answer "Yes, reopen it"
([note 4](SOURCE_NOTES.md)). The requirement and its acceptance are in
[the commit-and-push RFC](RFC-DRAFT-0002-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md).

| Before | After |
|---|---|
| "The author pushes with their own Git, so their credentials, hooks and remotes apply unchanged." | On the author's press BPR pushes with the author's own Git, so their credentials, hooks and remotes apply unchanged. |
| "Its cost is one step outside BPR: when the remote branch is missing or differs from the candidate, preparation names the exact push to run and observes the remote again rather than pushing." | When the hosted branch is missing or behind the candidate, preparation offers Push. Where the author declines the one-time question, or their own Git cannot be run, preparation names the exact push to run and observes the remote again. |
| "A credentialed `git push` would cross the credential-free Git adapter boundary and raise whether the checkout's own hooks and configuration run." (an alternative not chosen) | Chosen in part: a second runner, separate from the isolated one, runs the author's Git with the checkout's own hooks and configuration, on a local draft only and under a one-time question. The isolated adapter stays credential-free. |
| "This answer does not authorize exposing credentials, running arbitrary helpers or widening the installed-Git ports" | Unchanged for the installed-Git ports, which are not widened. The author-Git runner invokes the helpers and hooks the author's Git is configured with; BPR still reads, stores and passes no credential. |
| "BPR does not guess which dirty files to stage or commit. … preparation hands the specific remaining task to the author's editor/harness and recaptures the result." | BPR still does not guess: Commit… commits exactly the files the author selected into the candidate. Where Commit… is declined or unavailable, the remaining task is handed over as before. |
| "It does not add merge, force push, automatic rebase or automatic ready-for-review operations." | BPR itself adds no merge, rebase, force push or ready-for-review operation. Merge, squash and rebase exist only as agent history runs under the agent-changes RFC, each started by the author. Force push exists nowhere. |
| "Local PR preparation is not a general Git client." | Unchanged. BPR's own-Git operations are two: commit the selected files, and push the draft's branch. |
| AC01: "…When the remote branch is missing or differs from the candidate, BPR names the push to the hosted repository and pushes nothing itself; checkout data is unchanged throughout." | AC01: "…When the hosted branch is missing or behind the candidate, BPR offers Push under the commit-and-push RFC or, where that is declined or unavailable, names the push to the hosted repository. BPR pushes only on the author's press and never forces. Creating the pull request changes no checkout data." |

Preserved: the pull request is created from a verified hosted head through
the existing REST authority; the author's real commits are published
unchanged; the push names the resolved hosted repository and never a bare
`origin`; the exact preview and its authorization rules; evidence describes
the outgoing candidate; non-atomic publication, read-first journal and no
blind retry; no automatic post-publication push; the host qualification of
native creation. AC02 to AC07 are unchanged.

Consequences:

- [BPR-2029](../../../issues/10-backlog/BPR-2029-create-the-draft-pr-from-the-pushed.md)
  stays valid work: naming the push is still required as the fallback, and
  nothing in it performs a push. Its AC01 wording follows the table.
- [BPR-2031](../../../issues/10-backlog/BPR-2031-close-rfc-0111.md) carries
  AC01's text in its closeout checklist; the row follows the table.
- The adopted plan's `publish-prepared-draft` milestone has the exit "BPR
  names the push and performs none" and the boundary "the author pushes with
  their own Git". A changed exit is an explicit plan amendment, not a
  compatible refinement: the exit becomes "BPR names the push; it performs
  one only under the commit-and-push RFC", and the boundary names the
  reopened answer.
- D13 gains a successor entry pointing at D02 of
  [this pack's record](DECISIONS.md#decisions). D13's own text is history
  and is not rewritten.

Reconciliation: owned by `commit-and-push`
([DRAFT-0005](cards/DRAFT-0005-push-and-create-draft-pr-in-one-flow.md),
judged by [DRAFT-0006](cards/DRAFT-0006-close-commit-and-push.md)). If
`publish-prepared-draft` is implemented first, it implements the fallback
exactly as written today and nothing is lost.

## A2. RFC-BPR-0104: one more grant

Source: [RFC-BPR-0104](../../RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md),
"Builds and tests" and "Saying what is granted, and what is not".

Rationale: the owner's request that the agent fix findings and do most of
the changes on a local draft ([notes 1 and 3](SOURCE_NOTES.md)). The
requirement and its acceptance are in
[the agent-changes RFC](RFC-DRAFT-0003-agent-changes-in-a-local-pr-draft-worktree.md#the-grant).

| Before | After |
|---|---|
| "It is never available for a checkout the reviewer chose: BPR does not let an agent write into the reviewer's own working tree." | Unchanged for every pull request review. The one exception is a change run on a local PR draft under the Edits this worktree grant, where Build and run tests, when on for the agent, also applies in that worktree. |
| The grant line's three values: "Reads only", "Writes only in its scratch folder", "Builds and tests in its disposable checkout". | A fourth value on a local draft's Changes session: "Edits this worktree". |
| "Nothing here writes to GitHub or to a tracked file of the reviewer's repository." | Unchanged for the two allowances that RFC defines. The fourth grant is defined, with its question, in the agent-changes RFC. |

Preserved: the scratch folder and its bounds; builds only in the disposable
checkout for a pull request; the once-per-pull-request build question; no
network grant; no use of the word sandbox; tour runs read-only and one-shot
hooks tool-free. Its acceptance criteria are unchanged.

Consequences: [BPR-2007](../../../issues/30-in-progress/BPR-2007-agent-scratch-folder.md)
and [BPR-2008](../../../issues/10-backlog/BPR-2008-opt-in-builds-and-tests.md)
are unaffected in scope; the grant line they build must admit a fourth
value.

Reconciliation: owned by `agent-fixes`
([DRAFT-0007](cards/DRAFT-0007-worktree-grant-change-runs-keep-and-revert.md)).

## A3. RFC-BPR-0108: what may change the worktree

Source: [RFC-BPR-0108](../../RFC-BPR-0108-local-pr-drafts.md),
"Architecture and open boundaries".

| Before | After |
|---|---|
| "Source reads and indexes remain unchanged by BPR preparation." | Opening, reading, refreshing and preparing change no source and no index. The worktree changes only through Save, Commit…, a granted change run and Revert, each started by the author and defined in this pack's RFCs. |

Preserved, and still true as written: opening a folder "does not run
hooks/filters, stage files, create commits, switch branches, push, or start
an agent"; discarding a draft "never touches the worktree, its branches or a
linked pull request"; immutable candidates and the refresh that makes a
successor. AC01 to AC08 are unchanged.

Reconciliation: owned by `edit-and-save`
([DRAFT-0001](cards/DRAFT-0001-save-policy-and-worktree-write-port.md)).

## A4. RFC-BPR-0110: clarification, no requirement changed

[RFC-BPR-0110](../../RFC-BPR-0110-local-tours-and-description.md) says a
local draft's runs in the candidate workspace "are read-only, and this pack
adds no effect-capable session that shares it", and that "Local-draft
conversations stay read-only". Both stay true. A change run is a different
kind of run in a different place: the live worktree, not the candidate
workspace. The owning milestone adds one sentence there pointing at the
agent-changes RFC, so the two are not read as contradicting.

## A5. RFC-BPR-0112: clarification, no requirement changed

[RFC-BPR-0112](../../RFC-BPR-0112-optional-workflow-extensions.md) settles
ACTIONS-01: "BPR launches no supervised effectful action in this version."
That sentence answers a question about configured extension actions and
stays true of them. A change run is BPR's own agent run under a grant, which
the owner asked for afterwards and specifically. The owning milestone adds
one sentence there saying so.

## Not amended

RFC-BPR-0109 (evidence) and the tour, reporting and session RFCs this pack
builds on are used as written.
