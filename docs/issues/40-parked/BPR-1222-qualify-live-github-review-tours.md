---
schema: "implement-rfc.card.v1"
id: "BPR-1222"
title: "Qualify live GitHub.com review tours"
status: "Parked"
rfcs: []
depends_on: ["BPR-1013"]
metadata: {"standalone_follow_up":true,"owner_amendment":"r2-20260927-close-implemented-scope","originating_pack":"bpr-review-tour-v1","originating_rfcs":["RFC-BPR-0039","RFC-BPR-0040","RFC-BPR-0041","RFC-BPR-0042","RFC-BPR-0043"],"original_criteria":["AC40-07","AC41-04","AC41-06","AC42-08"],"source_card":"BPR-1013","excluded_from_terminal_scope":true}
parked_on: "human-review"
---

# BPR-1222: Qualify live GitHub.com review tours

## Owner amendment and retained obligation

Standalone follow-up under the owner's 2026-09-27 instruction: “Just close and land it on main anyways, can file the GHE stuff as blocked follow-ups.” The [r2 mandate](../../verification/RFC-BPR-0039-0043.owner-closeout-r2.md) accepts only the verified implementation. This card remains Blocked; no external qualification is passed by that scope change.

## Required course and blocker

Run the existing manual qualification course on an explicitly approved disposable GitHub.com PR with author and reviewer accounts. Qualify all eleven operation rows separately for native HTTPS and admitted macOS `gh api`, including pending line/multiline and off-hunk batches, mixed pending file-thread support, standalone fallbacks, pending comment/root edits, exact COMMENT submission, ambiguous-result reconciliation, published maintenance and separately previewed description edits. Record actual unsupported results without enabling unavailable operations. Observe pending/submitted visibility, website-only navigation/replies, updates and clean-profile reimport. Preserve independent server IDs, counts, actors, heads and synthetic bodies. No safe target or accounts have been supplied.

Use the [existing qualification course](../../verification/RFC-BPR-0039-0043.manual-course.md) and preserve the [candidate/evidence ledger](../../verification/RFC-BPR-0039-0043.md). No live mutation, inference, platform activation or unrelated predecessor work is authorized by this card alone.

## Original criteria retained verbatim

- **AC40-07:** Website-only reviewers can follow Start here → comments → Next/Previous → overview and reply normally; no invented heading fragment is needed.
- **AC41-04:** Native HTTPS and admitted `gh api` operations share normalized semantics and preserve their existing credential/authority boundaries.
- **AC41-06:** File-subject staging is qualified per exact operation; unbatchable destinations require an explicit compatibility plan and never an atomicity claim.
- **AC42-08:** A website-only user can continue and reply throughout completed updates; BPR reader deltas never mutate review status.

These original criteria require the direct evidence described above for this card's host/platform/provider slice. Shared criterion IDs on the GitHub.com and GHES cards partition host scope; neither card closes the other.

## Completion evidence

Record exact candidate, environment/host/provider identities, command or observed action, result and relevant human verdict. Unsupported/not-run rows retain their limitations; do not generalize fixture or historical evidence. This card starts with no passing qualification record.

## Owner designation of BPR-Test — 2026-09-30

On 2026-09-30 the owner wrote: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That supplies the disposable GitHub.com
repository, and with it the authority to create the draft PR, push, and later
advance or force-push its test branch, as the [manual course](../../verification/RFC-BPR-0039-0043.manual-course.md)
asks. The live target qualification milestone inspected the course and did
not run it, because it still needs two things the designation does not give:

- **A reviewer account.** Only `MikeMarcin` is signed in on the Mac. Step 3
  needs a browser with "only the reviewer account" to follow the tour and
  reply, and AC40-07 and AC42-08 are about that website-only reviewer. The
  author and reviewer must be different accounts.
- **An attended session.** Steps 1–8 drive the shipped application's tour
  gestures and the website by hand and record what a person sees: pending
  versus submitted visibility, website navigation and replies, B/L/R conflict
  choices, Stop and Resume. No unattended harness substitutes for them.

The card stays Blocked on `safe-github-tour-target`. Its next step is the
owner naming a second GitHub account for the reviewer role and scheduling an
attended course on BPR-Test. No live object was made for it.

## Reassessment after the BPR-Test designation — 2026-10-05

The owner's 2026-10-05 designation (root `AGENTS.md`: BPR-Test "for any
testing need") repeats the 2026-09-30 grant of the repository and does not
change either missing input. A read-only check through `gh api` as
`MikeMarcin` found `MikeMarcin` the only collaborator on BPR-Test and no
workflows (`actions/workflows` total 0), so a review there starts no Actions.

The course still cannot run with one account:

- Step 3, AC40-07 and AC42-08 need a browser signed in as a reviewer account
  that is not the PR's author. `MikeMarcin` is the only account and authors
  every PR it opens on BPR-Test.
- Steps 1–9 drive the shipped application's tour gestures and record what a
  person sees there and on the website. That is an attended course.

The card stays Blocked on `safe-github-tour-target`. No live object was made
for it.

**Owner question:** which second GitHub account (not `MikeMarcin`) should be
the reviewer on BPR-Test, signed into a browser during an attended run of the
course?

## Row observations from the BPR-1367 probe — 2026-10-09

[[BPR-1367]] made one review on `MikeMarcin/BPR-Test#45` (since closed, its
branch deleted) to check two operation rows' response shapes. These are
observations, not qualification: no row passes and the card stays Blocked.

- **Enumerate staged comments** (`GET .../reviews/{id}/comments`): no `line`,
  `side`, `start_line`, `start_side` or `subject_type`, pending or submitted.
  The pull request's comment collection lists no pending comment. BPR-1367
  reads a submitted review's lines from the collection and confirms pending
  stops' lines only after Submit.
- **Edit stop body** (`PATCH /pulls/comments/{id}`): 404 while the review is
  pending, 200 after Submit. GraphQL `updatePullRequestReviewComment` edits a
  pending comment. Filed as [[BPR-1368]]. Native publication cannot pass this
  row until that is fixed.

## Row evidence from the BPR-1368 live run — 2026-10-09

[[BPR-1368]] published one two-stop native tour on `MikeMarcin/BPR-Test#46`
(since closed, both branches deleted) at `2ad076bcb`. The production
`review_submission_session::publish_tour` ran over the production
`gh_cli_transport` (`gh` 2.101.0, macOS, account `MikeMarcin`) through the
hand-run `staged_review_session --live-tour` harness, not through the
application's gestures. This is row evidence, not qualification: no row
passes here and the card stays Blocked.

| Operation row | Transport | Observed |
|---|---|---|
| Create pending line batch | `gh api` | 200; one pending review, a RIGHT range (head 9 to 11) and a LEFT line (base 5) |
| Enumerate staged comments | `gh api` | pending: the review's own list, matched by ID, node, URL, author and body; submitted: lines read from the collection |
| Edit stop body, pending | `gh api graphql` | both stops edited by `updatePullRequestReviewComment` while pending; GitHub's answers matched the contract; no REST edit sent |
| Submit prepared review | `gh api` | `COMMENTED` with the final index body |

An independent `gh api` read afterwards found the review `COMMENTED`, stop 1 on
RIGHT 9 to 11, stop 2 on LEFT 5, and each stop's final body linking the other
stop and the overview. 48 REST and 4 GraphQL calls; `X-RateLimit-Remaining`
stayed at or above 4967 and 4972.

What this does not cover, so the card's course still needs:

- a second GitHub account as the website-only reviewer, and the attended
  course (AC40-07, AC42-08: Start here, Next/Previous, replies, updates);
- every row through native HTTPS (AC41-04); this run used `gh api` only;
- pending file-subject staging and standalone fallbacks (AC41-06), pending
  root (summary) edits, ambiguous-result reconciliation, published
  maintenance, separately previewed description edits and clean-profile
  reimport, none of which this run exercised;
- the shipped application's own Preview, Publish, Stop and Resume gestures.

## Owner session prepared, 2026-10-09 (main, Mac)

The owner question above (which second account is the reviewer) is asked in BPR-0144's wizard `bpr-second-account-20261009-v001` (session `c79054f3-14cf-44fe-9645-947ffd5f6d53`, state `/Users/jaeger/.mask/wizard/bpr-second-account-20261009-v001`): it names the account, then asks whether that account may be the website-only reviewer for this card's attended course. The course itself is its own later session; nothing of it ran.

## Owner wizard withdrawn, 2026-10-09

The owner added `MikeMarcinTest` as the second account, so the wizard
`bpr-second-account-20261009-v001` (session
`c79054f3-14cf-44fe-9645-947ffd5f6d53`) was stopped (`wizard.py stop`) and
paused with 0 of 7 steps answered. The account's capabilities on BPR-Test
(read, fork, pull request from the fork, review and reply; `pull` access
only) are on [BPR-0144](../60-done/BPR-0144-acting-identity-on-a-real-account.md#what-mikemarcintest-can-do-on-bpr-test-2026-10-09).

## Two-account reviewer course, 2026-10-09 (PDT; GitHub times UTC 2026-10-10)

The second-account rows, run by the agent: `MikeMarcin` authors, BetterPullReview
code publishes and maintains, `MikeMarcinTest` is the reviewer reading and
replying through `gh api` with its own token. This is row evidence; the card
does not pass from it.

**Target.** Draft `MikeMarcin/BPR-Test#50`, "bpr-2acct: BPR-1222 two-account
tour course (disposable)", id 4807255200, node `PR_kwDOTyvof88AAAABHojkoA`,
branch `bpr-2acct-1222-tour`, head `07aec64c3d7f5f14a94f7cc715c899f30f9e35dd`,
base `e2a44aa8fef704de7373463222b1872e796918c6`; the description ends in an
unrelated template paragraph. No workflows on BPR-Test.

**Candidate.** Product source `e964b5a83`. Publication and maintenance ran
through `staged_review_session --live-tour-course DIR PHASE` (harness
`400e6ddc2`): the production `review_submission_session` over the production
`gh_cli_transport`, gh 2.101.0, macOS, account `MikeMarcin`, one phase per
process, stopping if `X-RateLimit-Remaining` fell below 1500 (lowest seen
3960). Not the application's own gestures; see the gate below.

| Phase | Writes | Result |
|---|---|---|
| publish-pending: create pending review with three RIGHT line ranges (4–9, 12–20, 23–31 of `bpr-2acct/tour/parser.py`), edit all three by GraphQL, Stop | 1 POST + 3 GraphQL | review 5476795875 PENDING; comments 4235770230, 4235770244, 4235770251 |
| publish (new process): reconcile, submit COMMENT, separately authorised description edit appending the index link | 1 POST + 1 PATCH | COMMENTED; description ends `[Review tour](…#pullrequestreview-5476795875)`, template paragraph intact, still draft |
| update: reorder (last stop first), Stop after the first write | 1 PATCH | stop "Declared length" renumbered 1 |
| observe (new process): read-only reconcile | 0 | `ready`, one stop updated |
| resume (new process): reconcile, then finish | 2 PATCH + 1 PUT, no POST | published; same three comment IDs; root body last |

**Pending versus submitted visibility.** While review 5476795875 was
PENDING, `MikeMarcinTest` saw no review and no review comment on #50, and
`GET /pulls/comments/4235770230` answered 404; `MikeMarcin` saw his own
PENDING review; the anonymous rendered page carried no `discussion_r` anchor
for it. After Submit, `MikeMarcinTest` saw the COMMENTED review and the three
comments at RIGHT 4–9, 12–20, 23–31.

**Links (AC40-07, the checkable half).** Every link in the description, the
overview ("## Review tour — Start here") and each stop's Previous, Tour
overview and Next names a comment or review GitHub returned, and each
`#discussion_r…`/`#pullrequestreview-…` anchor is present in the anonymous
rendered conversation page; each pinned source permalink answers 200. The
same held in the partial state after the stopped update (two stops then
numbered 1, every link still resolving) and after the resumed update.

**Replies through updates (AC42-08, the checkable half).** `MikeMarcinTest`
replied to "Repeated headers" after publication (comment 4235775425), to "One
header" while the update was stopped (4235777903), and to "Declared length"
after it completed (4235780305). After the update every reply is in its
original thread, each thread holds two comments, unresolved, not outdated;
the tour comment IDs are the originals; reviews are the author's COMMENTED
root and the reviewer's three COMMENTED reply reviews; `reviewDecision`
stays null.

**BetterPullReview readers write nothing.** The shipped composition offscreen
(`live_second_account_course --course reader`, tests-on Release at
`e964b5a83`) opened #50 to "Pull request ready." and stayed 20 s; a GraphQL
read of the pull request, reviews and threads with their `updatedAt` was
identical before, during and after.

**Clean-profile reimport.** A fresh `bpr tour` data root (tests-off
`tour-cli` build at `e964b5a83`, gh transport) `open`ed #50 complete: root
review 5476795875 owned by `MikeMarcin`, stops 4235770251, 4235770230,
4235770244 in the published order, each stop's narrative found in its
server body, `prior_private_context` and `published_change_history` unknown,
no conflict. `preview` answered `publication_readiness: needs_qualification`,
`remote_writes: false`, and the server's comments, description and reviews
were unchanged by it. The imported narratives keep a leading and trailing
blank line from the rendered body; nothing was re-published from them.

**The application's own Preview, Publish, Stop and Resume** cannot write
live from either surface: production tour publication is unqualified by
construction (`tour_remote.cpp` builds its publication context with
`Qualified = false`; `live_review_submission_provider::publish_tour` returns
unavailable), and a tests-enabled composition keeps its composers read-only
without a fixture. `bpr tour preview` ran (above). Lifting the gate per row
is the owner's decision on this evidence.

**Found.** [BPR-1377](../60-done/BPR-1377-tour-cli-pinned-source-reads-fail-over-gh-sparse-trees.md):
`bpr tour add` cannot read pinned source over gh's sparse trees, so a live
tour cannot be authored from the terminal over gh.
[BPR-1378](../60-done/BPR-1378-a-stopped-tour-publication-reports-conflict.md):
a Stop between writes is reported as `conflict` in-process.

### Attended session for the website half

Session `0f7659f0e0e24a7e592e6f6266e04b7e`, state
`/Users/jaeger/.mask/attended-review-sessions/bpr-1222-website-reviewer-20261009-v001`,
root the main checkout; its loopback page URL carries a token and is handed
to the owner directly. One Accept/Reject question per criterion, all on #50
in a private browser window signed in as `MikeMarcinTest`:

1. `BPR-1222-AC40-07-navigation`: from the description's "Review tour", do
   Start here, Next, Previous and Tour overview each land where they say?
2. `BPR-1222-AC40-07-reply`: does a reply typed in the website's own Reply
   box post under its stop normally?
3. `BPR-1222-AC42-08-after-update`: after the reorder, do the stops read 1
   Declared length, 2 One header, 3 Repeated headers, with the three earlier
   replies still under their stops?

Cleanup at the owner's wrap-up: #50 was closed by `MikeMarcin` at 01:35:57 UTC
and `bpr-2acct-1222-tour` deleted. The closed pull request still renders the
tour, its threads and replies (page 200), so the three questions can be
answered on it; a reply on a closed pull request's review thread is still a
normal website reply. No `bpr-2acct` pull request or branch remains open on
BPR-Test; the fork `MikeMarcinTest/BPR-Test` holds only `main`.

### What remains on this card

- The three website verdicts above (human-review).
- Every operation row through native HTTPS (AC41-04): needs a native device
  sign-in by the owner.
- Rows not exercised here: off-hunk batches, pending file-subject staging and
  standalone file/PR fallbacks (AC41-06), pending root edits,
  ambiguous-result reconciliation, B/L/R website-conflict choices (step 4),
  retarget and membership removal (step 7), head advance (step 8).
- The application's own gestures for publication, after the owner decides
  which rows to qualify; BPR-1377 for the terminal route over gh.

The card moves from Blocked on `safe-github-tour-target` (target and both
accounts are now supplied) to Parked on `human-review`.

## Website verdicts, 2026-10-09 (session `0f7659f0e0e24a7e592e6f6266e04b7e`, hand-back 13)

The owner answered all three questions in a private window signed in as
`MikeMarcinTest` on `MikeMarcin/BPR-Test#50` (review 5476795875 after the
reorder; harness `400e6ddc2` on `e964b5a83`). Verbatim:

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1222-AC40-07-navigation` | Reject | "It works, but I really wish those tour stop links went to the comments on the files changed tab rather than the conversations tab." |
| `BPR-1222-AC40-07-reply` | Accept | "Replaied with "Some short text"" |
| `BPR-1222-AC42-08-after-update` | Accept | (none) |

So AC40-07's reply half and AC42-08's after-update half pass on that
candidate. AC40-07's navigation works but is rejected as to where it lands;
the amendment below answers it, and the owner re-checks it later on a fresh
tour. The live course was not re-run for it. The session was stopped
(`stop`; hosts 30139 and, after a restart, 85880) and its three candidates
released (`resume-development --all`, event 14).

## Owner amendment 2026-10-09: publication on and Files changed links

**Files changed links.** From the navigation note above. A tour's index
(`## Review tour — Start here`) and each stop's Previous and Next now link to
the comment on the Files changed tab, `https://github.com/<o>/<r>/pull/<n>/files#r<comment database id>`,
not the Conversation tab's `…/pull/<n>#discussion_r<id>`. Tour overview and the
description's Review tour link still name the review (`#pullrequestreview-<id>`)
on the Conversation tab, and compatibility stops (conversation comments,
`#issuecomment-<id>`) are unchanged. The anchor form was read, not guessed: one
anonymous fetch of `https://github.com/MikeMarcin/BPR-Test/pull/50/files`
(200, no redirect) carried each stop as `<div id="r4235770230" …>` with its
own permalink `href="#r4235770230"`, and likewise `r4235770244`,
`r4235770251`; one `gh api -i` read of comment 4235770230 beforehand showed
`X-RateLimit-Remaining: 4989`. `core::tour_stop_link` builds the link from the
`html_url` GitHub returns; `core::tour_link_identity` maps either form back to
that `html_url`, so import, reconciliation and the update planner match a tour
published before this change as well as after it, and an unchanged stop of an
older tour keeps its bytes. Pasting a `/files#r<id>` link into BetterPullReview
opens that review comment.

**Publication on.** The owner, 2026-10-09, verbatim: "yeah switch it on, let's
test it out for real". Live tour publication is enabled for the application and
for `bpr tour`, on both transports:

- `live_review_submission_provider::publish_tour` runs the production
  `review_submission_session` instead of returning unavailable;
- `tour_remote.cpp` builds the CLI's publication context qualified;
- the application's Tour Publish, Stop and Resume run against the live provider
  (`review_tour_publication_workflow::adopt` takes the qualification as passed;
  tests-on builds keep fixture-only qualification);
- `bpr tour capabilities` reports `remote_writes: true` and the gate as the
  typed `publish`; the local `preview` reports `complete` instead of
  `needs_qualification`.

Every per-write confirmation and refusal stays: the application's write
confirmation and one-use parent grant, each write's child authority, the CLI's
typed `publish` of the exact plan, `authority_changed`, `preview_changed`,
`remote_incomplete`, and the conflict, outcome-unknown and pending-review
refusals.

This is not a qualification pass. The pre-release itself qualifies it. The
rows observed on BPR-Test#46 and #50 above stay row evidence, and these stay
untried: every row through native HTTPS, off-hunk batches, pending file-subject
staging and standalone file/PR fallbacks (AC41-06), pending root edits,
ambiguous-result reconciliation, B/L/R website-conflict choices, retarget and
membership removal, head advance, GHES (BPR-1223), and the application's own
Publish, Stop and Resume gestures against GitHub. BPR-1377 (terminal authoring
over gh) and BPR-1378 (a Stop reported as `conflict`) remain open.

The card stays Parked on `human-review`: AC40-07's navigation needs the owner
to re-check the Files changed links on a newly published tour.

## Linux station run and main repair (bpr-main-linux, 2026-10-09)

**Main was red after `9cab2ad75`.** It had two gate failures:
- `architecture_boundaries` refused `src/core/src/review_tour_markdown.cpp`,
  because the core may hold no `https://` literal.
- `application_review_round_trip_composition_contract` reported that
  `github_application.cpp`'s digest had drifted. BPR-1373's `170c706c0` had
  also landed without a reseal.

The fix, on the coordinator's direction:
- **Core:** `pull_request_page` reads only the `…/pull/N` path. The host admits
  the URL's scheme and host before it reaches the codec, as
  `render_tour_index`'s contract already said.
- **Application:** `staged_review_publication.cpp`, which builds the index
  from the comment URLs GitHub returned, rewrites a link to its Files changed
  anchor only when it starts with `https://`. `review_tour_update.cpp`'s
  rewrite works on admitted remotes.
- **Core test:** the `http://` case of `files_changed_links` left the core
  list, since scheme admission is the host's.
- **Pins:** resealed in the same commit, over both drifted commits. The
  mutation pin did not move. No allowlist entry was added.

**Tour publication switched on (Linux, first run anywhere).** In
`build/dev-tour-cli` (`BPR_BUILD_TOUR_CLI=ON`, Debug, offscreen,
network-isolated), at the fix:
- all 7 `tour_cli_*` rows pass;
- all 9 `application_review_tour_publication_*` rows pass (native, compat,
  stop, description, conflict, unsupported, maintenance, reconciliation,
  manual);
- `architecture_boundaries`, the five `core_review_tour*` rows, the round-trip
  contract and `test_network_isolation_manifest` pass.

`tour_cli_loopback_provider`'s publish step reaches "Type publish to authorize
exactly this finite plan". Enter cancels it (exit 130), the fake GitHub
records no write, and the output says "no writes were authorized"
(`tour-cli-loopback:publish:pass:1.8s`). It also passed at `515720107`,
before the fix.
