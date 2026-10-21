---
id: "RFC-BPR-0089"
title: "Filter a pull request's review by its commits and set generated files aside"
status: "Proposed"
area: "BPR"
depends_on: ["RFC-BPR-0003", "RFC-BPR-0004", "RFC-BPR-0007", "RFC-BPR-0022", "RFC-BPR-0029"]
metadata: {"origin": {"date": "2026-10-20", "channel": "owner chat, relayed by the coordinator"}, "revision": "r3 2026-10-21, owner answers Q1-Q4 recorded", "authoring": "author-rfc, design only; no source changed", "owner_questions": "Q1-Q4 answered 2026-10-21 in attended session 63349fbf25e4b1e85a29c8dca65ef80a (closed); r1 session eeb8d789ecf1824d5a6f7ec7779284c7 superseded"}
---

# Filter a pull request's review by its commits and set generated files aside

Proposed, revision r3. This document changes no code. Revision r1 proposed a
separate "commit scope" comparison; the owner's direction below replaced it
with commit filters (r2). The owner answered r2's four questions on
2026-10-21 (Owner questions), and r3 records those answers in the design. No
owner question remains open.

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
  ([product contract](../architecture/product-contract.md)). The Overview shows
  only "N commits" (`src/adapters/qt/src/pull_request_overview_page.cpp`).
- **Status is a drawer with a single tab.** It sits on the right edge of a pull
  request's Overview, beside the Overview's file tree
  ([RFC-BPR-0085](RFC-BPR-0085-drawer-design-system.md), The Overview's Status
  drawer). Its tab row holds one tab, `Status` (`overviewStatusTabs` in
  `src/adapters/qt/src/overview_status_drawer.cpp`). In Review, files are
  listed in the Files drawer's tree, and Next and Previous file walk the
  host's ordered actionable set
  ([system architecture](../architecture/system-architecture.md), RFC-BPR-0005
  workbench allocation).
- **Only base and head trees are local.** A sparse clone takes
  `--revision=<commit> --depth=1 --filter=blob:none`
  (`src/adapters/github/src/gh_cli_transport.cpp`). Intermediate commits and
  their trees are absent. Blobs are read exactly per file, and nearby files are
  prepared ahead ([[BPR-1327]]).
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
- **Receipts attach to material.** A file receipt attaches to the comparison's
  material, never to a path or an epoch. The ledger's Excluded is the reviewer's
  own lasting statement and survives regeneration
  ([review ledger, Receipt semantics](../architecture/review-ledger.md#receipt-semantics)).
- **Line comments go on head only.** A line comment needs "the exact
  current/right endpoint" to be "the accepted pull-request head", and it is
  written with `commit_id` equal to that head
  ([RFC-BPR-0022](RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md#eligibility),
  [review round trip](../architecture/review-round-trip.md)). GitHub accepts
  any commit of the pull request as `commit_id`. Its documentation warns that a
  commit other than the latest may make the comment outdated.

## Owner direction (r2, 2026-10-21)

Asked through attended session `eeb8d789ecf1824d5a6f7ec7779284c7` for verdicts
on r1's eight questions, the owner sent an idea instead. The coordinator relayed
it verbatim:

> Maybe we put a commits tab on the home page in the same pane as status. And
> maybe it lists commits and we can filter to a selection or filter out a
> selection and of course reset/clear all filters. Multi select and context
> menus being the main form of operation. The filtering would be reflected in
> the file tree and all the othe operations like next file.

**Interpretation.** "Home page" is read as the pull request's Overview, where
Status lives. Home, the personal review page that lists pull requests, has no
Status pane and no single pull request's commits. "The same pane as status" is
read as a second tab, `Commits`, in the Status drawer's existing tab row.

The direction settles r1's question on where commits appear and how they are
operated. It also settles that commit selection is a filter over the review,
not a separate comparison. r1's questions on receipts, force push, the
Generated group and bulk commits follow from the filter model and from existing
contracts, so they were not asked again. Their rules appear below as proposals;
the owner may still change any of them. r1's questions on non-contiguous
presentation, comment anchoring and Linguist rules were asked again in filter
terms, and Clear all filters raised a new one. Those four (Q1 to Q4 below) were
answered on 2026-10-21 (Owner questions).

## Proposed behavior and boundaries

### The Commits tab

The Status drawer's tab row gains **Commits** beside **Status**. The tab lists
the pull request's commits in GitHub's order. Each row shows the headline,
author, short OID and the number of files the commit changes, plus a **merge**
or **bulk** label where one applies. A commit that changes at least **100
files** is labelled bulk. The label is a hint only and filters nothing by
itself. Commits never become nodes on the Updates line.

The list is multi-select: click, Shift-click for a range, ⌘/Ctrl-click to
toggle, and the platform's keyboard equivalents. The context menu is the main
way to act on a selection:

- **Show only selected**: the shown commits become exactly the selection.
- **Hide selected**: the selected commits are removed from the shown set.
- **Show selected**: the selected hidden commits are added back. It appears
  only when the selection contains a hidden commit.
- **Hide files these commits changed**: hides every path the selected commits
  changed, in every shown commit. This is a file filter, not a commit filter,
  and it is how "skip the regeneration" works when C3 also edits generated
  files.
- **Clear all filters**: every commit is shown again and the file filters are
  removed. Generated files stay hidden behind their own row, which is the only
  way to show them (owner, Q3).
- **Copy commit ID** and **Open on GitHub**.

Filters combine. The commit filter is one set of shown commits, which every
operation narrows or widens, and file filters add to it. A hidden commit's row
stays in the list, dimmed and marked hidden, so it can be shown again. With no
commit shown, the file tree is empty and offers Clear all filters. A merge from
the base branch is listed and labelled but cannot be shown on its own; its
first-parent diff is the base branch's own work. Commits not on the
first-parent chain sit under their merge and are filtered through it. A pull
request whose list is truncated at 250 has no commit filter and says why.

### What a filter drives

With no filter, the review is exactly today's comparison. `Whole PR` keeps
GitHub parity. With a filter active, every operation that lists or walks files
uses the filtered file set:

- the Overview's file tree and the Files drawer's tree;
- Next and Previous file, Open first file, and the actionable order;
- the file counts, the Status drawer's files-read progress, and preparation
  ahead ([[BPR-1327]]);
- Find in files, which says that it searches the filtered set.

Counts are labelled as filtered ("12 of 340 files, filtered by commits").
Whenever a filter is active, the Files drawer header and the Overview show a
chip that names it ("2 of 3 commits shown") and offers **Clear all filters**.
From Review, the chip opens the Commits tab, so a filter is never active out of
sight.

A filter applies to the commits the current comparison spans: the first-parent
commits after the comparison's left endpoint, up to its right. On `Whole PR`
those are all of the pull request's commits. Commits outside the comparison are
greyed in the list. When the comparison is not a first-parent range, for
example a reversed pair or a pair across a force push, the filter is suspended
and says why. The window keeps the filter with its comparison and restores it
with the window.

To support filters, the host acquires the commits on the head's first-parent
chain down to the merge base, with their trees but no blobs. It uses the
existing sparse acquisition, its quarantine and its bounds, and fetches only
when the Commits tab first needs them. Per-commit changes come from local tree
diffs with the existing bounded rename detection. Blobs are still read one file
at a time when a file is opened or prepared.

### Each file's diff is exact

The application never builds a tree that no commit contains. It does not
cherry-pick, synthesize or patch commits together. For each path P, let *T(P)*
be the commits in the comparison that change P. P is in the filtered set when
some shown commit is in *T(P)*. Let *first* and *last* be the earliest and
latest shown commits in *T(P)*.

- If no hidden commit in *T(P)* lies between *first* and *last*, P has **one
  exact diff**, from P at `parent(first)` to P at `last`. That diff is exactly
  the shown commits' change to P.
- Otherwise P is **split** (owner, Q2). P keeps one row in the file tree. Under
  it, P shows one exact diff per shown commit in *T(P)*, from P at `parent(c)`
  to P at `c`, in chain order. Each hidden commit in *T(P)* that falls between
  two of them is named in place ("C2 also changed this file; hidden"). Each
  per-commit diff is its own material for receipts.

Hide C2 in the owner's example. Every file C2 never touched is unchanged from
`Whole PR`, with identical material, so its read state carries over. A
generated file that C3 also edited is normally hidden anyway, as a generated
file or by "Hide files these commits changed" on C2. A contiguous set of shown
commits never splits a file.

### Generated files are set aside

Generated status belongs to the path, not to a commit, so it holds under every
filter and in `Whole PR`. A file is generated when the first of these rules
that says yes or no decides it:

1. The reviewer's per-review overrides, kept in the ledger: **Show as normal**
   and **Treat as generated** in the file tree's context menu.
2. `.gitattributes` `linguist-generated` at the pull request's head: set, unset
   (`-linguist-generated` or `=false`, which overrides everything below it) or
   unspecified. The rules are read from the head's root `.gitattributes` and
   from those in each changed path's ancestor directories, at most 64 files of
   at most 512 KiB each, with the existing reader extended by one attribute.
   [RFC-BPR-0083](RFC-BPR-0083-git-metadata-readers.md) may replace that reader;
   the meaning of the attribute is unchanged either way.

Nothing else marks a file generated. The owner rejected porting GitHub
Linguist's built-in generated rules (Q4), so lockfiles, minified files and
similar files without the attribute stay normal until the reviewer chooses
**Treat as generated** or hides them with a file filter.

Generated files leave the filtered file set, as filtered-out files do. The file
tree ends with one row, "N generated files hidden", showing their line totals.
Expanding it lists them, and opening one shows its diff on request. They are
not prepared ahead, not walked by Next file and not counted as remaining.
**Review anyway** returns a file to the set for this review. Generated status is
derived and is never written as the ledger's Excluded, which stays the
reviewer's explicit statement. No size or content heuristic hides anything by
itself.

### Viewed and reviewed state

Receipts follow the existing material rule unchanged. A file whose filtered diff
has the same material as its `Whole PR` diff shares its receipt. A file whose
diff the filter changed has its own receipt for that diff. Excluded applies
everywhere. A filter never changes GitHub's per-path Viewed state, which covers
the whole pull request.

### Comments and line anchoring

Threads keep their existing rule. A thread is marked inline only when the
comparison is proven to contain its exact endpoint revision; otherwise it
remains in the conversation drawer. Under a filter, a thread whose `commit_id`
or `original_commit_id` is the right endpoint of the file's diff is marked
there.

A file's filtered diff can end before head (its right side is C1 when C3 is
hidden), or start after the PR base. Publishing from such a diff amends
RFC-BPR-0022's eligibility only for filtered diffs (owner, Q1). The core maps
the selected right-side line from the diff's right endpoint to head through the
existing correspondence mapping:

- If the line is unchanged at head, the comment is published at head exactly as
  today, so it is current on GitHub.
- If it is not, the editor says so before Publish: the comment will be attached
  to commit `<short OID>`, which GitHub may show as outdated. It is then
  published with that `commit_id` and that commit's line.

The left side of a filtered diff is `parent(first)`, not the PR base, so a
left-side line follows the same rule mapped to the merge base, and otherwise
takes RFC-BPR-0022's file comment with a permalink. Every existing admission,
idempotency and reconciliation check still applies. Only the accepted target
commit widens, from "head" to "a commit of the accepted pull request on its
first-parent chain". A diff whose right side is head is commented on exactly as
today.

### Force push and rebase

Commit identities are OIDs, and filters name exact OIDs. After a force push the
commit list is read again. A filter whose commits are all still present is
unchanged. A filter that names commits that are gone stops applying to them,
and the chip says which commits are gone. The application offers, and does not
apply, the commits in the new history that carry the same change, for one-click
re-filtering. "The same change" means identical path and blob pairs, or
identical diff text where that is computed within existing bounds. A match on
headline and author alone is offered and labelled as a guess. File filters are
paths and are unaffected. Receipts carry over wherever the material is
identical. A new epoch records the push as before.

### Ownership

- **Core** decides the shown set and its operations, filter legality against
  the comparison, each path's diff, the generated decision and its reason,
  actionability, the comment target and rematch candidates. It decides them as
  pure functions over host-supplied touch sets, attribute rules and
  correspondences.
- **Host** keeps commit parents, acquires the first-parent commits and trees,
  runs the tree diffs, reads attribute blobs, persists overrides and the
  window's filter, and publishes with the decided `commit_id`.
- **Qt** draws the Commits tab, its multi-selection and context menus, the
  filter chip, the generated row and the reasons the core supplies. It runs no
  Git, matches no patterns and resolves no anchors.

Visual candidates for these surfaces load `frontend-design:frontend-design`
before they are drawn.

### Non-goals

Review Cycles and tours are out of scope. So are AI scopes, writing GitHub's
Viewed state, creating trees or commits, filtering beyond a pull request's 250
listed commits, and filtering by the base branch's commits.

### Alternatives rejected

- **A combined diff of cherry-picked shown commits.** It shows a tree no commit
  contains, and when C3 depends on C2 it conflicts or is quietly wrong.
- **A separate commit-scope comparison (r1).** It was replaced by the owner's
  filter direction.
- **Commits as nodes on the Updates line.** The owner set that line to show
  pushes only (RFC-BPR-0028).
- **Per-commit REST file lists as the source.** That costs one or more calls per
  commit against the shared rate limit, caps at 3,000 files and leaves nothing
  resident for the diff host.
- **Auto-hiding by size or similarity.** A large commit can be the change under
  review.
- **Writing Excluded automatically for generated files.** That would be a
  lasting statement the reviewer never made.

### Engineering questions (evidence, not owner choices)

- How GitHub presents a review comment whose `commit_id` is an earlier commit
  and whose line later changed. Measure this on BPR-Test before M4 is claimed.
- Which revision's `.gitattributes` GitHub's Files changed uses. Measure on
  BPR-Test before M2 claims parity. GitHub may also collapse files by
  Linguist's built-in rules, which this application does not apply (Q4), so
  parity is claimed only for files the attribute marks.
  **Measured 2026-10-21 (M2): the head commit's, everywhere.** BPR-Test pull
  request #112 (closed, both branches deleted) had a base branch whose root
  `.gitattributes` marked `base_only/**` and `both/**`, and whose tip, pushed
  after the head branched, added `merge_probe/.gitattributes` marking `g.txt`.
  C1 edited every probe file under the base's rules, C2 added
  `commit_only/** linguist-generated`, and C3 replaced the root rules with
  `head_only/**`, `both/**`, `*.gen` and `neg/keep.gen -linguist-generated`
  and added `nested/.gitattributes` marking `f.txt`. The anonymous Files
  changed page collapsed exactly `head_only/b.txt`, `both/c.txt`,
  `neg/other.gen` and `nested/f.txt` ("Some generated files are not rendered
  by default"): head's rules, with `-linguist-generated` returning a path to
  normal and a nested directory's file applied. `base_only/a.txt` (base and
  C1's rules) and `merge_probe/g.txt` (base tip and merge commit) stayed
  normal, so neither the base, the merge base nor the merge commit is read.
  The per-commit views `/pull/112/commits/C1`, `C2` and `C3` collapsed the
  same four paths: `commit_only/d.txt`, which only C2's own rules mark, stayed
  normal in C2's view. This matches "Generated status belongs to the path" and
  the head rule under Generated files are set aside. The measurement spent
  six REST calls; `X-RateLimit-Remaining` stayed above 4,990.
- The cost of acquiring trees for long chains in large repositories, within the
  existing quarantine and 96 MiB/128-entry thresholds.

## Milestones

1. **M1 Commits tab and contiguous filters.** Parents are kept, the Commits tab
   and its context menu work, first-parent trees are acquired, filters drive
   the trees, navigation, counts and the chip, receipts follow material, and
   the truncated, merge, suspended and force-push states are shown. Comments
   stay head-only.
2. **M2 Generated files and file filters.** Overrides, `linguist-generated`, the
   bulk label, "Hide files these commits changed" and the generated row.
3. **M3 Non-contiguous filters and rematch.** Split files and the rematch
   offers.
4. **M4 Comments from filtered diffs.** The head-or-commit anchoring rule and
   thread markers under filters.

## Acceptance

Unless a criterion says otherwise, each is observed on a BPR-Test pull request
of the owner's shape (C1 source, C2 regenerating at least 300 files, C3 mixing
both), through the shipped composition, offscreen and signed in, together with
a fixture course.

- **AC01:** The Commits tab sits beside Status and lists C1 to C3 with
  headline, author, OID and file count. With C1 selected, Show only selected
  makes the Overview tree, the Files drawer, Next file and the counts cover
  exactly C1's paths, with diffs equal to `git diff C1^ C1` path by path. Clear
  all filters restores `Whole PR` exactly. The truncated-at-250 and base-merge
  cases show their stated reasons.
- **AC02:** Hide selected on C2 leaves every file C2 did not touch with material
  identical to `Whole PR`, including its read state. The filter chip is visible
  in Overview and in Review. Combining operations (Show only {C1, C3}, then
  Hide C3, then Show selected C3) yields the stated shown set at each step.
- **AC03:** A file that hidden C2 changed between shown C1 and C3 keeps one row
  and shows `git diff C1^:P C1:P` and `git diff C3^:P C3:P`, with C2 named
  between them. No shown blob is absent from every commit of the chain
  (negative control: a synthesized tree fails the course).
- **AC04:** With `linguist-generated` on C2's paths, `Whole PR` and every
  filter leave those paths out of the trees, Next file and remaining counts,
  behind the generated row, unprepared, and Clear all filters leaves them
  there. `-linguist-generated` returns a path to normal. Review anyway, Show as
  normal and Treat as generated work and persist. Without attributes, a
  lockfile or minified file stays normal, and "Hide files these commits
  changed" on C2 hides those paths in C3 as well. Nothing is hidden before a
  rule or gesture says so.
- **AC05:** GitHub's Viewed state is unchanged by any filter action.
- **AC06:** After a force push that rewrites C3, the chip names the missing
  commit, and the same-change candidate is offered and applied only when the
  reviewer chooses it. File filters and identical-material receipts persist.
- **AC07:** With C3 hidden, a line of C1's diff that is unchanged at head
  publishes with `commit_id` = head at the mapped line. A line C3 changed
  publishes, after the stated warning, with `commit_id` = C1. Both reconcile
  once and weaken no admission check (negative control: a commit outside the
  pull request is refused before any write).
- **AC08:** The core decisions (shown-set operations, filter legality, each
  path's diff, generated precedence, comment target and rematch) run under
  `core-only` without Git or Qt. The architecture boundaries and documentation
  gates pass.

## Owner questions

The r1 session `eeb8d789ecf1824d5a6f7ec7779284c7` (external state
`/Users/jaeger/.mask/attended-review/owner-review-20261020-commit-scoped-review-v001`)
is closed as superseded. It holds no verdicts. Each candidate carries an
observation recording the supersession, its protections are released and its
host is stopped.

Session `63349fbf25e4b1e85a29c8dca65ef80a` (external state
`/Users/jaeger/.mask/attended-review/owner-review-20261021-commit-filters-v001`)
asked one question per decision, at candidate revision `rfc-bpr-0089-r2`. The
owner answered all four and handed back on 2026-10-21 (event 21), with no
notes. The session is closed and its host is stopped. The authenticated URL was
kept only in that external state.

| Question | Owner answer (2026-10-21) | Decision | Where applied |
|---|---|---|---|
| **Q1** Where a comment goes from a filtered diff whose right side is not head | Selected "Head when the line is unchanged there, otherwise the diff's commit after a warning" | `fc3a97ef` | Comments and line anchoring; AC07 |
| **Q2** How a file shows when a hidden commit changed it between two shown commits | Selected "Split into one exact diff per shown commit, naming the hidden one" | `504cfd1b` | Each file's diff is exact; AC03 |
| **Q3** Whether Clear all filters also brings back generated files | Selected "Generated files stay hidden behind their own row" | `5c23bdc0` | The Commits tab; AC04 |
| **Q4** Port a subset of GitHub Linguist's built-in generated rules into the core | Reject | `5e0cdb41` | Generated files are set aside; AC04 |

The rejected alternatives were: for Q1, always the commit the diff ends at, or
head only with a file comment; for Q2, one diff with the hidden commit's lines
marked, or leaving the file out; for Q3, Clear all filters showing generated
files. No owner question remains open. The rules described as proposals under
Owner direction (receipts, force push, the generated row and the bulk label)
stand unless the owner changes them.
