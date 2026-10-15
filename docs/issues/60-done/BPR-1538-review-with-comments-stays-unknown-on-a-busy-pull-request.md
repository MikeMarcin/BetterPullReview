---
schema: "implement-rfc.card.v1"
id: "BPR-1538"
title: "A review with comments stays unknown on every Check status on a busy pull request"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","origin":"owner report 2026-10-15 (relayed by the coordinator), installed app, a pull request on a GitHub Enterprise Server instance","governing_rfc":"RFC-BPR-0008","related_cards":["BPR-1345","BPR-1367","BPR-0614","BPR-0615","BPR-1475"],"reservation":"docs/issues/_reservations/BPR-1538--BPR-1538--35177d881942.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5a09d4f61","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1538-review-with-comments-stays-unknown-on-a-busy-pull-request.md","detail":"live_review_submission_provider row review_with_comments_reconciles_on_a_busy_pull_request (GitHub.com and GHES 3.18 targets, full-size GitHub-shaped pages) fails on 126ded865 at its first submit and passes at the fix; each of the three defects alone fails it at the named step."}},{"criterion":"AC2","outcome":"pass","revision":"5a09d4f61","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1538-review-with-comments-stays-unknown-on-a-busy-pull-request.md","detail":"Submit after a 504 confirms the approval over two collection pages; an unlisted review is unknown, then Check status in a later session at a pushed head confirms it with no second POST and pending() is empty."}},{"criterion":"AC3","outcome":"pass","revision":"5a09d4f61","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1538-review-with-comments-stays-unknown-on-a-busy-pull-request.md","detail":"A 504 with no review stays unknown on Check status with one POST and the attempt kept; a review holding a comment with other words stays unknown."}},{"criterion":"AC4","outcome":"pass","revision":"5a09d4f61","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1538-review-with-comments-stays-unknown-on-a-busy-pull-request.md","detail":"Live course on MikeMarcin/BPR-Test#70 (MikeMarcinTest approval with three comments over 100 earlier comments, then a push); --replay-busy-live runs the production session over GitHub's saved answers: unknown before the fix, published after, including Check status after the push. PR closed, branches deleted."}},{"criterion":"AC5","outcome":"pass","revision":"5a09d4f61","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1538-review-with-comments-stays-unknown-on-a-busy-pull-request.md","detail":"8 focused ctest rows pass on macOS; clang-format 22.1.8 check passes; clang-tidy clean on changed code; verify_documentation DOCUMENTATION_OK."}}]
---

# BPR-1538: A review with comments stays unknown on every Check status on a busy pull request

The owner reported on 2026-10-15, from the installed application: a review
submitted as Approve with line comments landed on GitHub, comments and
approval both, but the compose page said "Review status is unknown. Check
status to reconcile this attempt; do not submit it again.", and every Check
status gave the same answer. The pull request was on a GitHub Enterprise
Server instance. The instance is not named here, and the agent neither
contacted it nor looked for its name.

GHES is a supported host: the [product contract](../../architecture/product-contract.md)
admits GHES profiles on the token path and the configured `gh` path, and the
whole-review path compiles the GHES 3.18 REST contract (`/api/v3`). Native
publication to GHES had not been observed; no agent can reach one.

## What happens

The whole-review path ([RFC-BPR-0008](../../rfcs/RFC-BPR-0008-github-review-round-trip-interactions.md),
[review round trip](../../architecture/review-round-trip.md#whole-review-provider-boundary))
never trusts the POST's answer: after a possible send it reads the review
list and needs exactly one new review of mine at the original head with the
summary and verdict. A review that carries line comments ([[BPR-1345]]) is
also held to GitHub's comment collection
(`/pulls/{n}/comments?per_page=100&page=N`, [[BPR-1367]]): it is this review
only if GitHub holds exactly its comments. Check status repeats the same
reads and never POSTs.

The collection read could not succeed on a pull request with many review
comments, so neither the first answer nor any Check status could confirm the
review. Three defects, each enough on its own:

1. **A real page is over the parser's bounds.** A review comment as GitHub
   lists it carries its user, links, reactions and its diff hunk: 2.4 to
   5.3 KB and 62 JSON values each (captured on BPR-Test#44 and #70, and a
   public pull request). The collection page parser held each page to the
   ordinary mutation bounds, 256 KiB and 4,096 JSON values, so a page with
   about 66 comments, or fewer with long hunks, was refused. A full page of
   100 on #70 is 379,948 bytes. The `gh` path also capped its standard
   output at 288 KiB for every mutation exchange.
2. **GitHub names the next page by the repository's number.** The `Link`
   header reads
   `https://api.github.com/repositories/1328277631/pulls/70/comments?per_page=60&page=2`
   (live, 2026-10-15); on GHES the same under `https://HOST/api/v3`.
   [[BPR-1367]]'s pagination accepted only `/repos/OWNER/NAME/…`, which its
   fixtures used, so any pull request with more than 100 review comments was
   refused.
3. **Check status after a push held the comments to the new head.** The
   match compared each comment's `commit_id` with the head the session
   observes now and needed a current `line`. After a push GitHub moves a
   comment to the new head and shifts its line, or leaves it outdated with no
   line, so a review confirmed after the author pushed could never match.
   The architecture already promised reconciliation "including after head
   movement".

The first answer was unknown for the same reason, and can also be unknown
honestly: on #70 GitHub answered a 50-comment review with a 504 gateway
timeout and created it anyway. An unknown answer is correct while GitHub has
not listed the review; Check status is how it settles, and it could not.

Nothing in the read differs between GitHub.com and GHES 3.18: the review and
review-comment schemas in GitHub's GHES 3.18 and api.github.com REST
descriptions are the same, and the path prefix and origin were already
handled. GHES is where the owner met it because that pull request was busy.
The owner's own diagnostics are not on this Mac (its profile was last written
2026-10-11), so whether their pull request had more than about 60 review
comments, or more than 100, is not observed; the fix covers both, and a GHES
cause outside these reads, if there is one, would still show as unknown.

## Fix

- A page of the review list or the comment collection may be up to 3 MiB and
  32,768 JSON values (`maximum_review_mutation_list_response_bytes`,
  `maximum_review_mutation_list_values`); the `gh` transport takes the bound
  per operation (`maximum_review_mutation_wire_response_bytes`). Every other
  answer keeps 256 KiB and 4,096 values.
- A `Link` page may be named `/repos/OWNER/NAME/…` or
  `/repositories/ID/…` with this pull request's repository number, on the
  expected API origin; any other repository number or origin is refused as
  before. This also reaches the tour's paged reads, which share the parser.
- A carried comment is matched where it was written: `original_commit_id` is
  the request's head, `original_line` and `original_start_line` are the
  draft's, with its side, path and words. A server that names no original
  line is held to the current position at the request's head.
- Unchanged: one new exact review outside the baseline, a POST only once
  after its durable witness, Check status read-only, an unmatched or missing
  review stays unknown and keeps its drafts held.

## Acceptance

- **AC1, root cause:** reproduced with the production session over full-size
  GitHub-shaped answers, for GitHub.com and GHES: fails before the fix,
  passes after it.
- **AC2, landed review reconciles:** an approval with comments that GitHub
  created is confirmed on submit and on Check status, after a 504 answer, on
  a two-page collection, and after a push; the attempt is no longer restored.
- **AC3, unknown stays safe:** no review, or a review holding other comments,
  stays unknown on Check status with no second POST and the attempt kept.
- **AC4, live course:** an agent-run course on MikeMarcin/BPR-Test with a
  second identity, replayed through the production session.
- **AC5, gates:** focused contract and provider rows, clang-format 22, tidy on
  touched files, documentation gate.

## Evidence

### Live course (MikeMarcin/BPR-Test#70, 2026-10-15)

Agent-run through `gh`, `X-RateLimit-Remaining` read on every answer (4,999
to 4,988 on the final run). A base branch with a 20-line file and a head
branch removing one line, changing one and adding a 200-line file; pull
request #70 between them, authored by `MikeMarcin`. Two earlier reviews by
`MikeMarcin` carried 50 comments each; GitHub answered the second with a 504
gateway timeout and created it anyway. Then the second identity,
`MikeMarcinTest` (`GH_TOKEN` per command, no `gh auth switch`), sent the body
BPR compiles (`--compile-busy-live`): Approve, empty summary, a line, a
three-line range and a removed base line. GitHub listed it as `APPROVED` with
its three comments. The comment collection was two pages: page 1 is 379,948
bytes, and its `Link` names page 2 as
`https://api.github.com/repositories/1328277631/pulls/70/comments?per_page=100&page=2`.
At creation `original_line` and `original_start_line` equal `line` and
`start_line` on both sides. After a push inserting five lines, GitHub moved
the comments to the new head (`commit_id`) and five lines down (`line` 150 to
155, range 161–163 to 166–168), keeping `original_commit_id` and
`original_line`. The pull request was closed and both branches deleted.

`--replay-busy-live` runs the production `review_submission_session` over
GitHub's saved answers, headers included, through the network-denied test
runner:

| Phase | Before the fix (`126ded865`) | After |
|---|---|---|
| Submit, GitHub's 200 answer, two pages | unknown | published, review id matches, 1 POST, 2 pages |
| Answer lost, review not yet listed; Check status later | (not reached) | unknown, then published, no second POST, attempt cleared |
| Same, after the push; Check status at the new head | (not reached) | published; with only the old comment match it stays unknown |

### Deterministic rows

`live_review_submission_provider`, new row
`review_with_comments_reconciles_on_a_busy_pull_request`, for GitHub.com and a
GHES 3.18 target (`ghe.example.invalid`, `/api/v3`): two earlier reviews with
130 complete comments and a reply (page 1 about 450 KB), an Approve with three
comments answered by a 504.
- Submit confirms the review over both pages; nothing is left to reconcile.
- Not listed yet, then a push moves one comment and outdates another: the
  first answer is unknown; Check status in a later session at the new head
  confirms it with no second POST, and the attempt is no longer restored.
- A 504 with no review: unknown, Check status stays unknown, still one POST,
  the attempt kept.
- A review holding one comment with other words: unknown.

Failing-before controls, each defect alone (the rest of the fix in place):
the ordinary 4,096-value bound, `/repos/`-only links, and the current-head
comment match each fail the row at the step the card names; with no fix the
row fails at its first submit.

`github_mutation_contract` `test_real_collection_pages`: a full page of 100
complete comments (over 256 KiB) is read for GitHub.com and GHES; a
repository-number next link is followed, prev/first are read; another
repository's number, or this number on another origin, is refused; a page over
3 MiB is refused; only the review list and comment reads take the larger bound.
`gh_cli_transport`: the standard output bound of each reconciliation read is
its operation's answer bound.

### Gates

- `ctest` (macOS, Debug without debug info for disk space): `gh_cli_transport`,
  `github_mutation_contract`, `live_review_mutation_provider`,
  `live_review_mutation_restart_recovery`, `live_review_submission_provider`,
  `live_review_submission_restart_recovery`, `staged_review_session`,
  `staged_review_restart`: 8/8 pass.
- clang-format 22.1.8 check of the eight touched C++ files passes.
- clang-tidy with warnings as errors on the touched translation units: no
  finding in changed code (the two shared test headers
  `fixture_json.hpp` and `canonical_json_expectations.hpp` report their
  existing `pragma once` and naming findings, unchanged).
- `python3 scripts/verify_documentation.py --root .` passes.

Not run: the application-level composition rows
(`application_start_review_contract`,
`application_review_submission_composition_public_path`); the workflow and
host code is unchanged and the disk had under 2 GB free. Windows and Linux
runs of these rows are owed by the stations.

## Owner's stuck review

The approval and its comments are on the server; nothing needs sending again.
Discard review is disabled while a submission awaits confirmation, so it
cannot delete anything now. A build with this fix confirms it on Check status
(or when the pull request is reopened), marks its comments published and
closes the page. Until then the page stays on Check status, which is harmless.
If Check status still says unknown on a build with this fix, the cause is
outside these reads, and the agent needs the owner to say how many review
comments that pull request has and whether it was pushed to after the review.
