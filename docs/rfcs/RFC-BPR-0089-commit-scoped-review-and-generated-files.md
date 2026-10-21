---
id: "RFC-BPR-0089"
title: "Review chosen commits of a pull request and set generated files aside"
status: "Proposed"
area: "BPR"
depends_on: ["RFC-BPR-0003", "RFC-BPR-0004", "RFC-BPR-0007", "RFC-BPR-0022", "RFC-BPR-0029"]
metadata: {"origin": {"date": "2026-10-20", "channel": "owner chat, relayed by the coordinator"}, "authoring": "author-rfc, design only; no source changed", "owner_questions": "Q1-Q8 open in attended session eeb8d789ecf1824d5a6f7ec7779284c7 (see Owner questions)"}
---

# Review chosen commits of a pull request and set generated files aside

Proposed. This document changes no code. The owner answers Q1 to Q8 before any
milestone those answers govern is implemented.

## Problem and grounded current behavior

The owner reported, verbatim (2026-10-20):

> I came across a PR that changed stuff and then regenerated hundreds of files
> based on that change. The committed the change as one commit and the
> regeneration as a second and asked to review the first commit's files. Except
> in our app currently there's no way to view commits or their files. Now what
> they asked for isn't great because they made follow-up changes and now those
> are in a 3rd commit so yeah I don't know how to deal with a situation like
> this exactly.

In this document's examples that pull request has three commits: **C1** changes
the source, **C2** regenerates hundreds of files, and **C3** adds follow-ups to
both source and generated files.

What the application does today:

- **Comparisons are epoch pairs.** An epoch is an update (push) to the pull
  request ([RFC-BPR-0029](RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)).
  The named comparisons are `Whole PR`, `Previous upload -> latest` and
  `My last reviewed -> latest`, and any two resident anchors form an explicit
  pair ([git and epoch model, Comparison policy](../architecture/git-and-epoch-model.md#comparison-policy)).
  `Whole PR` compares from the merge base GitHub names ([[BPR-1560]]). The
  Updates line has one node per push, "never a commit"
  ([RFC-BPR-0028](RFC-BPR-0028-review-workbench-direction.md), Epoch line). If
  C1 to C3 arrived in one push, no comparison isolates any of them.
- **Commits are read and then dropped.** The provider reads
  `pulls/{n}/commits` and keeps only OID, node, author and headline
  (`github_commit_record`, `src/application/include/bpr/application/github_read_model.hpp`;
  `src/adapters/github/src/github_read_normalizer.cpp`). It drops parents. The
  read is capped at the 250 records the endpoint returns
  ([product contract](../architecture/product-contract.md)). Overview shows
  only "N commits" (`src/adapters/qt/src/pull_request_overview_page.cpp`).
- **Only base and head trees are local.** A sparse clone takes
  `--revision=<commit> --depth=1 --filter=blob:none`
  (`src/adapters/github/src/gh_cli_transport.cpp`). Intermediate commits and
  their trees are absent. Blobs are read exactly per file, and nearby files are
  prepared ahead ([[BPR-1327]],
  [system architecture](../architecture/system-architecture.md)).
- **The file list is GitHub's whole-PR list.** Each row resolves
  `commit + path -> blob` at the two selected endpoints.
- **Nothing recognizes generated files.** The core has a `.gitattributes`
  reader (`workspace_attribute_rules` in
  `src/core/include/bpr/core/review_cycle_ignore.hpp`) that answers only
  `filter`, for Review Cycle capture. Its replacement is under evaluation in
  [RFC-BPR-0083](RFC-BPR-0083-git-metadata-readers.md). GitHub publishes no
  generated flag. Introspection on 2026-10-20 found exactly `additions`,
  `changeType`, `deletions`, `path` and `viewerViewedState` on GraphQL
  `PullRequestChangedFile`. The REST `pulls/{n}/files` record has `sha`,
  `filename`, `status`, `additions`, `deletions`, `changes`, `patch` and three
  URLs. The REST commit record does carry `parents`.
- **Excluded is a reviewer's own lasting statement.** The ledger's exclusion
  is set by the reviewer and survives regeneration ("a generated artefact stays
  uninteresting when it is regenerated"). A file receipt attaches to the
  comparison's material, never to a path or an epoch
  ([review ledger, Receipt semantics](../architecture/review-ledger.md#receipt-semantics)).
- **Line comments go on head only.** A line comment needs "the exact
  current/right endpoint" to be "the accepted pull-request head", and it is
  written with `commit_id` equal to that head
  ([RFC-BPR-0022](RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md#eligibility),
  [review round trip](../architecture/review-round-trip.md)). GitHub accepts
  any commit of the pull request as `commit_id`. Its documentation warns that a
  commit other than the latest may make the comment outdated.

## Proposed behavior and boundaries

### Commit list and commit scope

The application keeps each commit's parents, then lists the commits in GitHub's
order with headline, author, short OID and the number of files the commit
changes. The list sits in Overview and in a Commits popover beside the Updates
line. Commits never become nodes on the Updates line (**Q8**). Click selects
one commit, Shift-click selects a range, and ⌘/Ctrl-click toggles one commit.
"All commits" returns to `Whole PR`, and the selection can also be changed from
the keyboard.

A **commit scope** is a new kind of comparison beside epoch pairs. It is pinned
to exact OIDs and never moves to other commits. A window keeps and restores it
the way it keeps an explicit pair. While a scope is active the Updates line
shows no bracket, and a chip names the scope ("Commits 1, 3 of 3"). The chip
also returns to `Whole PR`.

To support scopes, the host acquires the commits on the head's first-parent
chain down to the merge base, with their trees but no blobs. It uses the
existing sparse acquisition, its quarantine and its bounds, and fetches only
when a scope is first requested. Per-commit changes come from local tree diffs
with the existing bounded rename detection. Blobs are still read one file at a
time when a file is opened or prepared. A scope's file list is the union of the
paths its commits change. It is not GitHub's whole-PR list, and it is labelled
as the scope's own.

Merge commits are listed and labelled. A merge from the base branch cannot be
selected, because its first-parent diff is the base branch's own work. Inside a
range it counts as a skipped commit. Commits that are not on the first-parent
chain appear under their merge and are selected through it. A pull request
whose list is truncated at 250 offers no commit scope and says why.

### Non-contiguous selections are shown truthfully

The application never builds a tree that no commit contains. It does not
cherry-pick, synthesize or patch commits together. For each path P, let *T(P)*
be the commits on the chain that change P. Let *first* and *last* be the
earliest and latest selected commits in *T(P)*. **Recommended (Q1):**

- If no unselected commit in *T(P)* lies between *first* and *last*, P shows
  **one exact diff**, from P at `parent(first)` to P at `last`. That diff is
  exactly the selected commits' change to P, because the commits between them
  do not touch P.
- Otherwise P is **split**. It shows one exact diff per selected commit, under
  P's row, and each skipped commit that changed P in between is named in place
  ("C2 also changed this file; not shown"), with a control that adds it to the
  scope.

In the owner's example, scope {C1, C3} shows every source file once, as the
change C1 and C3 made together. A generated file that C3 touched again is
split, and it is normally set aside anyway (next section). A contiguous scope
never splits a file, and a single-commit scope is simply each file's diff
against the commit's parent.

### Setting generated and bulk files aside

Generated status belongs to the path, not to a commit. A file set aside stays
aside in every scope and in `Whole PR`, so "review the change, skip the
regeneration" works even when C3 mixes both. With generated files set aside,
`Whole PR` alone already answers the owner's case. A commit scope is needed only
when the regeneration cannot be recognized, or when the reviewer wants to read
commit by commit.

A file is generated when the first of these rules that says yes or no decides it:

1. The reviewer's **per-review overrides** in the ledger: "Show as normal",
   "Treat as generated", and **"Set aside this commit's files"**. The last one
   stores the paths the commit changed, not the commit, so it still holds after
   a force push. It also covers C3's later changes to those paths (**Q4**).
2. `.gitattributes` `linguist-generated` at the pull request's head: set, unset
   (`-linguist-generated` or `=false`, which overrides everything below it) or
   unspecified. The rules are read from the head's root `.gitattributes` and
   from those in each changed path's ancestor directories, at most 64 files of
   at most 512 KiB each, with the existing reader extended by one attribute.
   [RFC-BPR-0083](RFC-BPR-0083-git-metadata-readers.md) may replace that reader;
   the meaning of the attribute is unchanged either way.
3. Optional (**Q5**): an attributed, pinned subset of GitHub Linguist's built-in
   generated rules (MIT), kept as data in the core. They cover lockfiles,
   minified files, source maps and protoc output by path, and first-line
   markers such as `Code generated … DO NOT EDIT.` and `@generated`. A first-line
   marker is checked only on blobs already read within existing bounds, so a
   file can move into the group after the list appears without moving the
   cursor.

A commit that changes at least **100 files** is labelled **bulk** in the commit
list, and the label offers "Set aside this commit's files". No size or content
heuristic sets anything aside on its own (**Q4**).

**Recommended presentation (Q3):** generated files collapse into one
**Generated** group at the end of the file list, showing counts and line totals.
They are not prepared ahead, they are not in the actionable order, and they do
not count as remaining. Opening one shows the diff on request. "Review anyway"
returns that file to the actionable order for this review. Generated status is
derived and is never written as the ledger's Excluded, which stays the
reviewer's explicit statement. A generated file the reviewer explicitly
included or accepted stays visible where it is.

### Viewed and reviewed state

A diff shown under a commit scope has its own comparison material. Receipts
follow the existing material rule unchanged (**Q6**). Seen, Read and Accepted
for a scope's diff carry over to `Whole PR`, or to another scope, only where
the material is identical. For example, a file that only C1 touched, in a
first-commit scope, shows exactly the `Whole PR` material. A split file's
per-commit diffs are separate materials. Exclusion survives everywhere, as it
does today. A commit scope never changes GitHub's per-path Viewed state, which
covers the whole pull request.

### Comments and line anchoring

Threads keep their existing rule. A thread is marked inline only when the
comparison is proven to contain its exact endpoint revision. Otherwise it
remains in the conversation drawer. Under a scope, a thread whose `commit_id`
or `original_commit_id` is the right endpoint of the file's diff is marked
there.

Publishing from a scope amends RFC-BPR-0022's eligibility only for scopes, and
only for the right side. **Recommended (Q2):** the core maps the selected line
from the scope's right endpoint to head through the existing correspondence
mapping. If the line is unchanged at head, the comment is published at head
exactly as it is today, so it is current on GitHub. If the line is not at head,
the editor says so before Publish: the comment will be attached to commit
`<short OID>`, which GitHub may show as outdated. It is then published with that
`commit_id` and that commit's line. The left side of a scope diff is
`parent(first)`, not the PR base, so a left-side line follows the same rule
mapped to the merge base, and otherwise takes RFC-BPR-0022's file comment with
a permalink. Every existing admission, idempotency and reconciliation check
still applies. Only the accepted target commit is widened from "head" to "a
commit of the accepted pull request on its first-parent chain".

### Force push and rebase

Commit identities are OIDs. After a force push the commit list is read again.
A scope whose commits are all still present is unchanged. A scope with any
commit gone becomes visibly unavailable and names the missing commits. It stays
viewable, labelled "no longer in the pull request", for as long as its objects
remain in the cache, and it cannot be commented on. **Recommended (Q7):** the
application offers, and does not apply, the commits in the new history that
carry the same change, for one-click re-selection. "The same change" means
identical path and blob pairs, or identical diff text where that is computed
within existing bounds. A match on headline and author alone is offered and
labelled as a guess. Receipts carry over wherever the material is identical.
Set-aside path sets are unaffected. A new epoch records the push as before, and
`Previous upload -> latest` remains the way to compare two pushes.

### Ownership

- **Core** decides scope legality and pinning, the split rule per path, the
  generated decision and its reason, and actionability. It also decides comment
  target mapping and rematch candidates, as pure functions over host-supplied
  touch sets, attribute rules and correspondences.
- **Host** keeps commit parents, acquires the first-parent commits and trees,
  runs the tree diffs, reads attribute blobs, persists overrides in the ledger
  and publishes with the decided `commit_id`.
- **Qt** shows the commit list, the selection gestures, the scope chip, split
  rows, the Generated group and the reasons the core supplies. It runs no Git,
  matches no patterns and resolves no anchors.

Visual candidates for these surfaces load `frontend-design:frontend-design`
before they are drawn.

### Non-goals

Review Cycles and tours are out of scope. So are AI scopes, writing GitHub's
Viewed state, creating trees or commits, scoping a pull request beyond its 250
listed commits, and choosing scopes by the base branch's commits.

### Alternatives rejected

- **A combined diff of cherry-picked selected commits.** It shows a tree no
  commit contains, and when C3 depends on C2 it conflicts or is quietly wrong.
- **Commits as nodes on the Updates line.** The owner set that line to show
  pushes only (RFC-BPR-0028), and commits within a push have no push time.
- **Per-commit REST file lists (`GET /repos/{o}/{r}/commits/{sha}`) as the
  source.** That costs one or more calls per commit against the shared rate
  limit, caps at 3,000 files, and leaves nothing resident in the cache that the
  diff host reads. The same commit list serves only as a cross-check.
- **Auto-hiding by size or similarity.** A large commit can be the change under
  review, so an automatic guess would hide real work without the reviewer
  knowing.
- **Writing Excluded automatically for generated files.** That would be a
  lasting reviewer statement the reviewer never made.

### Engineering questions (evidence, not owner choices)

- How GitHub presents a review comment whose `commit_id` is an earlier commit
  and whose line later changed, on Files changed, in the conversation and in
  the commit view. Measure this on BPR-Test before M4 is claimed.
- Which revision's `.gitattributes` GitHub's Files changed uses, and whether it
  also collapses Linguist's built-in generated files. Measure on BPR-Test
  before M2 claims parity.
- The cost of acquiring trees for long chains in large repositories, within the
  existing quarantine and 96 MiB/128-entry thresholds.

## Milestones

1. **M1 Commit list and contiguous scopes.** Parents are kept, the list appears
   in Overview and the popover, first-parent trees are acquired, single and
   range scopes work, the scope file list and chip appear, receipts follow
   material, and the truncated, merge and force-push states are shown. Comments
   remain head-only.
2. **M2 Generated and bulk.** Overrides, `linguist-generated`, the bulk label,
   "Set aside this commit's files" and the Generated group with its
   actionability, in every scope and `Whole PR`. Linguist rules follow if Q5 is
   accepted.
3. **M3 Non-contiguous scopes and rematch.** The split rule and rematch offers.
4. **M4 Comments from a scope.** The Q2 rule and thread markers under scopes.

## Acceptance

Unless a criterion says otherwise, each is observed on a BPR-Test pull request
of the owner's shape (C1 source, C2 regenerating at least 300 files, C3 mixing
both), through the shipped composition, offscreen and signed in, together with
a fixture course.

- **AC01:** The commit list shows C1 to C3 with headline, author, OID and file
  count. Choosing C1 shows exactly C1's paths and diffs, which equal
  `git diff C1^ C1` path by path. The truncated-at-250 and base-merge cases
  show their stated reasons and offer no scope.
- **AC02:** The scope {C1, C3} shows each source file once, as
  `git diff C1^:P C3:P`. A file C2 also changed between them is split into
  C1's and C3's exact diffs and names C2. No shown blob is absent from every
  commit of the chain (negative control: a synthesized tree fails the course).
- **AC03:** With `linguist-generated` on C2's paths, `Whole PR` and every
  scope put those paths in the Generated group, which is not prepared ahead, is
  not in the actionable order and is not counted as remaining.
  `-linguist-generated` returns a path to normal. "Review anyway" and "Show as
  normal" work and persist.
- **AC04:** Without attributes, C2 is labelled bulk. "Set aside this commit's
  files" sets those paths aside in `Whole PR` and in C3's scope. Nothing is set
  aside before that action.
- **AC05:** A Read receipt given in a scope appears in `Whole PR` exactly when
  the two materials are equal, and is absent otherwise. GitHub's Viewed state
  is unchanged by any scope action.
- **AC06:** After a force push that rewrites C3, the scope names the missing
  commit and stays viewable while its objects are retained. The same-change
  candidate is offered and only applied when the reviewer chooses it.
  Set-aside paths and identical-material receipts persist.
- **AC07:** From C1's scope, a line unchanged at head publishes with
  `commit_id` = head at the mapped line. A line C3 changed publishes, after the
  stated warning, with `commit_id` = C1. Both reconcile once, and no admission
  check is weakened (negative control: a commit outside the pull request is
  refused before any write).
- **AC08:** The core decisions (scope legality, split rule, generated
  precedence, comment target and rematch) run under `core-only` without Git or
  Qt. The architecture boundaries and documentation gates pass.

## Owner questions

Session `eeb8d789ecf1824d5a6f7ec7779284c7` is in the external state directory
`/Users/jaeger/.mask/attended-review/owner-review-20261020-commit-scoped-review-v001`,
with one question per decision. Its status was open (unanswered) when this
document was written. The authenticated URL is kept only in that external
state.

- **Q1** How a non-contiguous scope shows a file. Choose one of three:
  per-file one exact diff, or split per commit where a skipped commit touched
  the file (recommended); always split per commit; or one combined diff from
  `parent(first)` to `last` with the skipped commits' lines marked.
- **Q2** Where a comment from a scope goes. Choose one of three: head when the
  line survives, otherwise the viewed commit (recommended); always the viewed
  commit; or head only, with a file comment and permalink when the line is gone.
- **Q3** Accept or Reject: generated files collapse into a Generated group that
  is not prepared, actionable or counted as remaining until opened or "Review
  anyway".
- **Q4** Accept or Reject: "Set aside this commit's files" sets those paths
  aside in every commit of this review. A bulk label at 100 files suggests it,
  and no heuristic sets anything aside by itself.
- **Q5** Accept or Reject: port an attributed subset of GitHub Linguist's
  built-in generated rules (MIT) into the core as data.
- **Q6** Accept or Reject: receipts from a scope count elsewhere only where the
  material is identical, and a scope never changes GitHub's Viewed state.
- **Q7** Accept or Reject: after a force push the scope stays pinned and
  unavailable, and the application offers commits carrying the same change for
  one-click re-selection.
- **Q8** Accept or Reject: commits are listed in Overview and in a popover
  beside the Updates line, never as nodes on it.
