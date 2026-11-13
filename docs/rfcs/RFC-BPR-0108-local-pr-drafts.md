---
id: "RFC-BPR-0108"
title: "Prepare a local PR draft from a worktree"
status: "Auto-Verified"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0002","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: []
---

# Prepare a local PR draft from a worktree

## Problem and grounded current behavior

The author's usual starting point is a worktree containing a change. Current
Review Cycles opens a folder/file, captures its source, and initially displays
full source. Its first captured revision is not a comparison with the intended
PR target. Home gives that feature a prominent Review Cycles entry. This does
not immediately answer what the author proposes to merge. See [S4–S5 and
S8](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

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
open ([S17](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)), can supply the working directory
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
remote-tracking refs can mislead ([S10](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)). BPR
follows local file-path remotes read-only, for at most four hops, to the clone
whose remote names a hosted repository, and shows that chain beside the target.
Each hop's configuration is read under the existing clone-discovery bounds and
rules, which follow no include ([S17](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)).
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
steps. (Owner amendment, 2026-11-12: Evidence is cut from 0.5.0 and
[RFC-BPR-0109](RFC-BPR-0109-author-evidence.md) is Deferred; the 0.5.0 areas
are Changes, Description and Tour.) Authors can move among them, return to an editor, save and resume without
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
the existing native Home design and [proposed flow](assets/v0-5-0-prerelease-targets-v1/local-drafts/UX-FLOW.md).

### Architecture and open boundaries

Core owns draft/candidate identity, explicit selection and stale-result policy.
The application owns discovery, capture, persistence and lifecycle; Qt owns
presentation and gestures. Persistence must recover a complete prior or new
candidate, never a partial ready revision. Source reads and indexes remain
unchanged by BPR preparation. How existing branch commits become remote objects
belongs to the [publication RFC](RFC-BPR-0111-draft-publication-and-continuity.md#native-branch-decision);
nothing in this RFC pushes or publishes.

### Fitted to this pack's other decisions

These rules come from the [reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md); its row numbers are given.

- **Home's sections do not change (X01).** [RFC-BPR-0098](RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md) keeps Needs me, My pull
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
  with no chat open, as [RFC-BPR-0105](RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md) opens every pull request on Comments.

### Owner amendment, 2026-11-12: the pull request window and Home 1

The owner chose mockup A and Home variant 1 and asked for author state
([accepted design](../design/local-draft-pr-window-v1/README.md)):

> So I think A + Home 1, but A needs a little bit of extra love for being able
> to edit locally. Local draft is more of a state than a mode since we're
> talking about letting this same mode be used for addressing PR feedback.

This changes the requirements above as follows; the rest stands.

- **The window.** A local draft opens in the pull request window, not on a
  page of its own. Overview holds the title, description and files; Review
  holds the proposed diff and the accepted Tour panel; Discuss and Threads
  wait until a pull request exists. The candidate is one pop-up in the
  Comparing slot whose menu holds the captured candidates, Refresh and
  Discard…. Create Draft PR… stands where Submit review stands, and a Status
  drawer tab holds the publish steps. Once the pull request is created the
  same window becomes it. "Changes, Description and Tour are working areas"
  becomes: Overview and Review are the window's own modalities, and the
  draft's notes, tour and status are drawer tabs.
- **Opening state (X23).** A local draft opens on Review with no chat open,
  in place of Changes: Review is where the changes are.
- **Home (X01, AC06).** No band and no visible In preparation title: each
  local draft is one 26 px row in the Open row's grid, first on Home, saying
  Local draft, with In preparation as the group's accessible name. Prepare
  Pull Request… is a button in the Open row. Home's four sections are
  unchanged.
- **Author state.** A window on the author's own worktree (a local draft, or
  a pull request opened by its author) shows the branch beside the title with
  ahead and behind counts, a third view (Edit) in the review bar's view
  picker, and a Local Changes drawer tab with three comparisons (Uncommitted,
  Not pushed, Whole PR), staging, commit and push. Those behaviours are
  specified in [RFC-BPR-0113](RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md)
  and [RFC-BPR-0114](RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md)
  as amended the same day, and are delivered on this lane.
- **What may change the worktree.** "Source reads and indexes remain
  unchanged by BPR preparation" becomes amendment A3 of the
  local-draft-authoring pack: opening, reading, refreshing and preparing
  change no source and no index; the worktree and its index change only
  through Save, Stage, Unstage, Commit and Push, each pressed by the author.

AC06 reads: Home makes Prepare Pull Request… and each local draft reachable
in the Open row while existing Review Cycles remain reachable and unchanged,
with native keyboard and accessibility and ordinary pull request navigation
preserved. AC08's last clause reads: a resumed local draft opens on Review
with no chat open.

## Acceptance criteria

- **AC01:** From Home, a signed-out author selects a clean Git worktree, including an isolated local clone of their primary clone, and reaches the correctly based proposed branch diff with source, target and Local draft identity visible, without launching an agent or changing the checkout.
- **AC02:** Dirty and untracked material is distinguishable from the branch candidate; only selected working files enter a captured candidate, with exact bytes and honest coverage/transform qualifications.
- **AC03:** Forks, tracking branches, local-clone remote chains, missing bases, detached heads and ambiguous destinations cannot produce a falsely identified target or comparison; a valid local-clone chain resolves to its hosted remote without fetching, and specific recovery leaves existing preparation intact.
- **AC04:** Notes and candidate material survive restart and external edits; refresh, competing completions, branch switches, moved folders and path reuse do not silently retarget the draft.
- **AC05:** The folder chooser and local harness handoff reach the same resumable draft behavior; context submission neither publishes nor creates a duplicate session by a path-only identity guess.
- **AC06:** Home makes PR preparation and In preparation prominent while existing Review Cycles remain reachable and unchanged, with native keyboard/accessibility and ordinary PR navigation preserved.
- **AC07:** Discarding a draft and the existing profile resets remove its private preparation and leave the worktree, branches and any linked pull request untouched; a full draft or candidate store refuses new admission with a reason and evicts nothing silently.
- **AC08:** With local drafts present, Home's four sections and their authored rows are as they are without local drafts; a local draft associated with a pull request appears once, as that pull request's row with a mark that opens its preparation; and a resumed local draft opens on Changes with no chat open.

## Proposed delivery

[Workspace identity](../issues/60-done/BPR-2019-discover-the-worktree-and-prospective-pr-comparison.md), [durable draft](../issues/60-done/BPR-2020-persist-local-pr-draft-candidates-and-author.md),
[Home/workbench flow](../issues/60-done/BPR-2021-open-and-resume-local-pr-drafts-from.md), and [closeout](../issues/60-done/BPR-2022-close-rfc-0108.md)
form the second milestone in the [plan's default order](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
Sharing remains the owner's first requested delivery; its pending closeout does
not block independent local-draft engineering within an authorized mandate.

## Implementation record

Milestone `local-draft-entry`, product candidate `3585dedc7`; evidence in the
[verification ledger](../verification/RFC-BPR-0108.md). Decisions made within
the behaviour above, for the owner to see:

- **Thresholds.** 64 drafts; 32 candidates per draft; per candidate 4096
  changed paths, 512 included working files, 16 MiB per kept file and
  256 MiB in all; 1 GiB in the store; 512 notes of 16 KiB; four local-remote
  hops. A full store or draft refuses with the amounts and evicts nothing.
- **Candidate material.** Both sides of every changed file are kept in the
  profile's store when a candidate is captured, so a candidate stays readable
  after a rebase or garbage collection in the worktree. A side over 16 MiB is
  named and its bytes are not kept.
- **Home.** "Prepare Pull Request…" is a push button in the title case of
  Home's other buttons, with In preparation beneath it, above Home's sections.
  Review Cycles keeps its section, unchanged, below Home's sections.
- **The mark (X02).** A pull request a draft was associated with shows
  "Prepared here" ("Draft, prepared here" for a GitHub draft) in its row's
  chip, and its row menu offers Open Local Preparation.
- **Areas.** Only Changes (with notes) is shown; Description and Tour come
  with their RFCs and are not shown before then. Evidence is not part of
  0.5.0 (owner amendment, 2026-11-12).
- **Handoff.** `bpr draft open [PATH] [--context TEXT]`
  (guide `docs/development/draft-cli.md` on the lane `claude/v0-5-0-prerelease-targets-52265d`); context becomes a note from the
  harness.
