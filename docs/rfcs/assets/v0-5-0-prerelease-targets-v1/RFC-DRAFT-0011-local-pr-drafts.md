---
id: "RFC-DRAFT-0011"
title: "Prepare a local PR draft from a worktree"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0002","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: []
---

# Prepare a local PR draft from a worktree

## Problem and grounded current behavior

The author's usual starting point is a worktree containing a change. Current
Review Cycles opens a folder/file, captures its source, and initially displays
full source. Its first captured revision is not a comparison with the intended
PR target. Home gives that feature a prominent Review Cycles entry. This does
not immediately answer what the author proposes to merge. See [S4–S5 and
S8](local-drafts/CURRENT-BEHAVIOR.md).

The owner accepted the term **local PR draft**, the Home-to-folder-to-diff flow,
and draft PR creation as its final publication action. The owner also asked that
Review Cycles lose Home prominence; this proposal preserves that existing feature
as a separate local-review workflow rather than renaming or migrating its records.

## Proposed behavior and boundaries

### Entry from Home or an authoring harness

Home retains opening/reviewing existing pull requests and adds **Prepare pull
request…** as its principal author entry. It opens a native folder chooser for a
Git checkout or linked worktree. A `bpr` command, following the existing tour
and cycle command-line conventions including their behavior while the desktop is
open ([S17](local-drafts/CURRENT-BEHAVIOR.md)), can supply the working directory
and optional initial context to the same flow; it does not create a different
kind of draft or authorize publication. Its spelling is an implementation
detail; its capability must be discoverable and its input an explicit workspace,
not a global last-used folder.

BPR observes the worktree, branch and repository identity and suggests a target
repository/branch from an explicit prior choice or available default-branch
facts. It does not equate a feature branch's tracking branch with the PR target,
assume `origin` is the destination, or assume `main` exists. Forks, missing
bases, detached HEADs and ambiguous destinations produce a specific selection or
recovery task. A non-Git folder is directed to the existing local-review
feature; it is not silently initialized as a repository.

An isolated worktree that is a local clone of the author's primary clone is a
supported starting point, not a recovery case. It is the setup the enterprise
overview describes: its `origin` is the primary clone's path, and its
remote-tracking refs can mislead ([S10](local-drafts/CURRENT-BEHAVIOR.md)). BPR
follows local file-path remotes read-only, for at most four hops, to the clone
whose remote names a hosted repository, and shows that chain beside the target.
Each hop's configuration is read under the existing clone-discovery bounds and
rules, which follow no include ([S17](local-drafts/CURRENT-BEHAVIOR.md)).
Objects borrowed through Git alternates (`clone --shared`) are read by the same
isolated observation. A cycle, an unreadable hop, a fifth hop or a chain that
never reaches a hosted remote is a specific recovery task. Following the chain
fetches nothing and changes no clone.

Ordinary local preparation requires no GitHub account, remote write, clean tree,
commit or agent. Available local facts suffice to start; missing target material
is explicitly unavailable until deliberately acquired. Opening a folder does not
run hooks/filters, stage files, create commits, switch branches, push, or start
an agent. Narrow Git metadata/object observations belong to the Git adapter and
application, never to the existing read-only filesystem workspace adapter or Qt.

### The first useful screen is the proposed diff

The workbench identifies the workspace, source branch, target and **Local draft**
state, and displays the branch's proposed change. Record the observed target tip,
source head and actual comparison base separately. The comparison is the one a
pull request shows: the source head against its merge base with the observed
target tip. Expose unavailable ancestry or more than one merge base rather than
inventing a comparison. A target that has moved
since observation is not represented as current by a cached label.

All committed branch changes are the initial scope. Working-tree edits are
listed separately and enter the proposed candidate only by an explicit author
selection. First-version selection of working edits is whole-file; untracked
files are opt-in and existing exclusions still apply. Arbitrary extraction of
committed hunks into another branch is outside this draft flow. An included file
is captured as exact bytes/mode; it is not automatically staged or committed.
The review clearly distinguishes the branch head from a candidate containing
uncommitted material. Publication rules belong to the publication RFC.

Capture and compare immutable observations. Ongoing editor or agent activity
cannot silently move the source underneath a note, evidence record or tour stop.
Possible changes offer refresh; an accepted refresh creates a successor candidate
and retains the previous one. Failed or unstable capture leaves the prior
candidate available. Preserve capture-quality, filter/LFS, symlink, sparse and
missing-material qualifications from the established material contracts.

### A durable authoring subject

A local PR draft owns a stable local identity, source/target observations,
candidate revisions, author intent, notes, unresolved questions and later RFCs'
evidence, description and tour. It belongs to the local BPR profile without a
fabricated account or PR number. It is private profile data: the existing reset,
purge and diagnostics-export rules treat it as they treat private notes and
local cycles. Local PR drafts, review-comment drafts, tour
drafts and Review Cycles remain distinct concepts. Reuse bounded material and
diff services where they fit; do not make GitHub authority fields optional
throughout the existing application.

Changes, Evidence, Description and Tour are working areas, not mandatory wizard
steps. Authors can move among them, return to an editor, save and resume without
finishing a one-shot agent run. The no-agent path is complete. This RFC supplies
the local shell and changes/notes; subsequent RFCs add the other usable areas.
Unavailable later features must not look completed in an intermediate build.

Home's **In preparation** list resumes local drafts by title, repository and
branch. Reopening the same identified worktree offers its applicable draft;
branch switching, another worktree or path reuse cannot silently retarget it.
Moving a folder uses an explicit rebind with material comparison. If an actual
PR is subsequently found for the same intended branch/destination, offer a
verified association instead of assuming another PR must be created. Remote
discovery does not block offline local reading.

The author can discard a local draft. Discarding asks first, removes only BPR's
private preparation, and never touches the worktree, its branches or a linked
pull request. Drafts and their retained candidates are bounded: implementation
records the thresholds, nothing is evicted silently, and a full store refuses a
new draft or candidate with a storage reason while existing work stays readable.

Review Cycles keeps its data, commands and File > Open Project… entry. Replace
its prominent standalone Home card with the author entry and preparation list;
retain a compact secondary route to existing local cycles. Do not relabel old
cycles as PR drafts, remove them, or force a migration. Exact placement follows
the existing native Home design and [proposed flow](local-drafts/UX-FLOW.md).

### Architecture and open boundaries

Core owns draft/candidate identity, explicit selection and stale-result policy.
The application owns discovery, capture, persistence and lifecycle; Qt owns
presentation and gestures. Persistence must recover a complete prior or new
candidate, never a partial ready revision. Source reads and indexes remain
unchanged by BPR preparation. How existing branch commits become remote objects
belongs to the [publication RFC](RFC-DRAFT-0014-draft-publication-and-continuity.md#native-branch-decision);
nothing in this RFC pushes or publishes.

### Fitted to this pack's other decisions

These rules come from the [reconciliation record](RECONCILIATION.md); its row numbers are given.

- **Home's sections do not change (X01).** [RFC-DRAFT-0001](RFC-DRAFT-0001-home-says-whose-move-my-pull-request-is.md) keeps Needs me, My pull
  requests, Waiting on others and Following and gives each authored row one
  next move. Prepare pull request… and In preparation sit with the author
  entry above those sections. They add no section among them and change no
  row in them.
- **One row once a pull request exists (X02; confirmed by the owner, 2026-11-10).** After a local draft is associated with a pull request, Home
  shows that pull request's row with its next move and a mark that it has
  local preparation; the mark opens the preparation. In preparation lists
  only local drafts with no pull request.
- **The word Draft (X03).** In My pull requests the reason word Draft means
  a GitHub draft pull request, and its sentence says so. In preparation
  labels its rows Local draft.
- **Opening state (X23; confirmed by the owner, 2026-11-10).** Durable
  preparation resumes; view state does not. A local draft opens on Changes
  with no chat open, as [RFC-DRAFT-0008](RFC-DRAFT-0008-a-review-opens-fresh-and-a-submission-always-ends.md) opens every pull request on Comments.

## Acceptance

- **AC01:** From Home, a signed-out author selects a clean Git worktree, including an isolated local clone of their primary clone, and reaches the correctly based proposed branch diff with source, target and Local draft identity visible, without launching an agent or changing the checkout.
- **AC02:** Dirty and untracked material is distinguishable from the branch candidate; only selected working files enter a captured candidate, with exact bytes and honest coverage/transform qualifications.
- **AC03:** Forks, tracking branches, local-clone remote chains, missing bases, detached heads and ambiguous destinations cannot produce a falsely identified target or comparison; a valid local-clone chain resolves to its hosted remote without fetching, and specific recovery leaves existing preparation intact.
- **AC04:** Notes and candidate material survive restart and external edits; refresh, competing completions, branch switches, moved folders and path reuse do not silently retarget the draft.
- **AC05:** The folder chooser and local harness handoff reach the same resumable draft behavior; context submission neither publishes nor creates a duplicate session by a path-only identity guess.
- **AC06:** Home makes PR preparation and In preparation prominent while existing Review Cycles remain reachable and unchanged, with native keyboard/accessibility and ordinary PR navigation preserved.
- **AC07:** Discarding a draft and the existing profile resets remove its private preparation and leave the worktree, branches and any linked pull request untouched; a full draft or candidate store refuses new admission with a reason and evicts nothing silently.
- **AC08:** With local drafts present, Home's four sections and their authored rows are as they are without local drafts; a local draft associated with a pull request appears once, as that pull request's row with a mark that opens its preparation; and a resumed local draft opens on Changes with no chat open.

## Proposed delivery

[Workspace identity](cards/DRAFT-0031-discover-the-worktree-and-prospective-pr-comparison.md), [durable draft](cards/DRAFT-0032-persist-local-pr-draft-candidates-and-author.md),
[Home/workbench flow](cards/DRAFT-0033-open-and-resume-local-pr-drafts-from.md), and [closeout](cards/DRAFT-0034-close-local-pr-draft-entry-and-persistence.md)
form the second milestone in the [plan's default order](milestone-plan.json).
Sharing remains the owner's first requested delivery; its pending closeout does
not block independent local-draft engineering within an authorized mandate.
