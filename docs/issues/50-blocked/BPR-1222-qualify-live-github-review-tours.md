---
schema: "implement-rfc.card.v1"
id: "BPR-1222"
title: "Qualify live GitHub.com review tours"
status: "Blocked"
rfcs: []
depends_on: ["BPR-1013"]
blocked_by: "safe-github-tour-target"
metadata: {"standalone_follow_up":true,"owner_amendment":"r2-20260927-close-implemented-scope","originating_pack":"bpr-review-tour-v1","originating_rfcs":["RFC-BPR-0039","RFC-BPR-0040","RFC-BPR-0041","RFC-BPR-0042","RFC-BPR-0043"],"original_criteria":["AC40-07","AC41-04","AC41-06","AC42-08"],"source_card":"BPR-1013","excluded_from_terminal_scope":true}
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
