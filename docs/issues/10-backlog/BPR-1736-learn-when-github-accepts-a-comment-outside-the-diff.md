---
schema: "implement-rfc.card.v1"
id: "BPR-1736"
title: "Learn when GitHub accepts a line comment outside the diff, and place it natively there"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-1735"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1736--BPR-1736--f61711c4c46d.json","kind":"design","origin":"Owner, 2026-10-22, on BPR-1735's option P: find clear capabilities for comment-anywhere support","phase":"research and design done; awaits the owner's decisions","related_cards":["BPR-1735","BPR-1716","BPR-1715","BPR-1706","BPR-1680"],"related_rfcs":["RFC-BPR-0089"]}
---

# BPR-1736: Learn when GitHub accepts a line comment outside the diff, and place it natively there

## Owner request

The owner, 2026-10-22, verbatim:

> I think we can probably find clear capabilities for comment anywhere
> support... I dunno if gh apis can tell us support or if we have to base it
> off version but we should be able to figure out when we can use it and when
> we need to work around

[[BPR-1735]]'s option P counts a line as natively placeable only within
`github_comment_context_lines` (3) of a change. This card decides how the
application learns that a host also accepts a line further out, so that it
uses a native comment there and works around only where it must.

## Answer

No GitHub host accepts an off-hunk line through its public API today, and no
GitHub API says whether it does. Neither the host's name nor its version is a
sound positive signal: GitHub.com has had comment-anywhere in its web
interface for every user since January 2026, and its API still refuses it.
The one reliable signal is GitHub's own answer to a real native write at a
line the core has proved off-hunk. The design learns from that answer, keeps
it per host profile, repository and server version, and forgets it when the
version changes. Version is a cache key, never evidence.

## What GitHub supports (2026-10-22)

### GitHub.com

- **Web: yes.** The new Files changed page added "comment anywhere in a
  changed file" in public preview on 2025-09-25, rolled out per repository
  ([changelog](https://github.blog/changelog/2025-09-25-pull-request-files-changed-public-preview-now-supports-commenting-on-unchanged-lines/)).
  That post says "API support is currently limited, but comments on unchanged
  lines will be returned by existing APIs and in webhook events". On
  2026-01-22 the new page became the default for all users, "Comment on any
  line" included
  ([changelog](https://github.blog/changelog/2026-01-22-improved-pull-request-files-changed-page-on-by-default/)).
  GitHub Mobile followed on 2026-02-03
  ([changelog](https://github.blog/changelog/2026-02-03-github-mobile-comment-on-unchanged-lines-in-pull-request-files/)).
- **Public API: no.** No changelog post announces API support. The REST
  reference still defines `line` as "The line of the blob in the pull request
  diff" ([REST](https://docs.github.com/en/rest/pulls/comments)). REST API
  version `2026-03-10` exists (`GET /versions` returns `["2026-03-10",
  "2022-11-28"]`), and its breaking-changes list does not touch review
  comments ([breaking changes](https://docs.github.com/en/rest/about-the-rest-api/breaking-changes)).
  No preview media type for it is documented.
- **Measured** on BPR-Test #125, below: every public write path refuses a
  line outside the three-line context.

### GitHub Enterprise Server

- **No version supports it.** The GHES 3.21 release notes carry this errata:
  "These release notes previously included a note about pull request
  reviewers being able to comment on any line of a changed file, not just
  lines within the diff. The note was added prematurely and has been removed.
  We will republish it when the feature is ready. [Updated: 2026-06-23]"
  ([3.21 notes](https://docs.github.com/en/enterprise-server@3.21/admin/release-notes)).
- GHES 3.22 is current (3.22.0 on 2026-09-08, 3.22.2 on 2026-10-06). Its
  notes say nothing about commenting on unchanged lines
  ([3.22 notes](https://docs.github.com/en/enterprise-server@3.22/admin/release-notes)).
  GHES 3.19 names only the new Files changed page, in public preview. 3.20
  says nothing.
- The GHES 3.22 REST reference keeps the same `line` definition
  ([GHES 3.22 REST](https://docs.github.com/en/enterprise-server@3.22/rest/pulls/comments)),
  and its `AddPullRequestReviewThreadInput` has the same fields as GitHub.com's
  ([GHES 3.22 GraphQL](https://docs.github.com/en/enterprise-server@3.22/graphql/reference/pulls)).
- The owner's GHES 3.20.8 refused it (BPR-1715). That fits all of the above.

## Measurement: BPR-Test #125

The throwaway PR #125 added an 80-line file on a fresh base branch and changed
line 10 on the head, so lines 7 to 13 are the hunk and line 14 is the first
line past it. Probe `bpr-pending-am/20261022T190334Z`, as MikeMarcin, with no
account switch. It made 29 REST and GraphQL calls; `X-RateLimit-Remaining`
stayed at 4,981 or above. The one comment that was created (line 13) was
deleted at once, the pending review was deleted, the PR was closed with no
review or review comment left, and both branches were deleted.

| Request | GitHub.com's answer |
|---|---|
| REST single comment, line 13 (last context line) | 201; the comment's `diff_hunk` is `@@ -7,7 +7,7 @@`; deleted |
| REST single comment, line 14 | 422 `Validation Failed`, one error: resource `PullRequestReviewComment`, field `pull_request_review_thread.line`, code `custom`, "could not be resolved" |
| REST single comment, line 60, API version 2022-11-28 | the same 422 |
| REST single comment, line 60, API version 2026-03-10 | the same 422 (`X-GitHub-Api-Version-Selected: 2026-03-10`) |
| REST single comment, line 60 with `subject_type: line` | 422 schema error: "`subject_type` is not a permitted key" when `line` is given |
| REST pending review (no event), line 60, API version 2026-03-10 | 422 `{"message":"Unprocessable Entity","errors":["Line could not be resolved"]}` |
| GraphQL `addPullRequestReviewThread(subjectType: LINE)`, line 60, into a pending review | 200, `thread: null`, no `errors` entry: **refused silently** |
| The same with no `subjectType`; with line 14; with `pullRequestId` only | 200, `thread: null`, no `errors` |
| The same at line 13 | 200, a thread on line 13; the review then held that comment alone |

So the boundary is exactly P's rule: three lines of context, and nothing
past it, through any public write. GraphQL's refusal names no error at all;
any later use of GraphQL threads (BPR-1735's option G) must treat a null
`thread` as a refusal and re-read the pending review.

## Discoverable signals

| Signal | What it tells | Tells supported from not? |
|---|---|---|
| `GET /meta` `installed_version` | The GHES version (documented on GHES; absent on GitHub.com, whose `/meta` has no such key) | No. It names the server, not what the API accepts |
| `X-GitHub-Enterprise-Version` response header | The same, on every GHES API answer ([GHES docs](https://docs.github.com/en/enterprise-server@3.22/apps/sharing-github-apps/making-your-github-app-available-for-github-enterprise-server)) | No, for the same reason. Neither transport keeps it today (`gh_cli_transport.cpp` and `native_https_transport.cpp` keep content type, links and rate-limit headers) |
| `X-GitHub-Api-Version-Selected`, `GET /versions` | The REST contract in force | No. 2026-03-10 refuses as 2022-11-28 does |
| GraphQL schema: `AddPullRequestReviewThreadInput`, `PullRequestReviewThreadSubjectType` (`LINE`, `FILE`), `DiffSide` (`LEFT`, `RIGHT`) | Fields only | No. The full GitHub.com schema has no field, enum or flag about unchanged lines, outside-diff or commentable lines; GHES 3.22's input is the same. Unlike BPR-1680's `undefinedField`, nothing is missing on a host without the feature |
| A capability or feature endpoint | None exists for this | - |
| Readable off-hunk comments made in the web page | The web supports it | No. GitHub.com is the counterexample, and RFC-BPR-0022 already refuses this as write proof |
| GitHub's answer to a native write at a line the core proved off-hunk | That exact operation, repository and server | **Yes.** A created comment at that line proves it accepted; a fully received refusal naming the line proves it refused |

A version would be sound evidence only if GitHub documented API support from a
named version. It has not, and on GitHub.com web support came at least nine
months ahead of API support, so a future GHES note about the web page would
not be API evidence either. If GitHub ever documents API support per version,
a recorded contract may name it then (decision D2).

## Design

### The fact

One tri-state fact, `off_hunk_native_line`: `unknown`, `supported` or
`unsupported`. It is RFC-BPR-0022's `off_hunk_line_creation` with a wider,
explicit generalization contract. Each value carries its source (observed
acceptance, observed refusal, or recorded contract), the time it was
observed, the server version and the API version it was observed under, and
the comment (repository, pull request, path, line) that produced it.

### Sources and precedence

1. **Observed, exact key.** The newest observation for the key below wins. A
   refusal after an acceptance demotes at once. An acceptance after a
   refusal promotes only through a new native attempt, which happens only
   after the refusal expires.
2. **Recorded contract.** It supplies classifier shapes only: what a
   refusal from this server contract looks like (the GitHub.com tuple in the
   table above, measured on PR2, #124 and #125). It supplies no state,
   unless the owner accepts D2.
3. **Nothing else.** Host name, product, version, schema and readable
   comments never set the state.

With no observation the fact is `unknown`.

### Key and cache

Stored durably in the profile's SQLite database, so one refused attempt is
paid once per key rather than once per session. RFC-BPR-0022's session cache
(`review_publication_capability_cache`, 128 entries, 300 seconds, one exact
anchor) stays as it is for its own purpose.

The key is:

- profile identity and profile revision, host, and account (as
  `github_capability_authority_key` has);
- **server identity**: product, and on GHES the `installed_version` from
  `GET /meta`, read once per connection by the existing capability probe
  (one more GET), and checked against `X-GitHub-Enterprise-Version` on the
  mutation answer that produced the evidence. Evidence is recorded under the
  version the answer names. An unparsable or missing version on GHES makes
  the fact `unknown` and records nothing;
- the REST API version sent;
- the operation (single REST comment, REST review, GraphQL thread);
- the repository's database ID, because GitHub.com rolled the web feature
  out per repository;
- the classifier revision.

Lifetime: on GHES, until `installed_version` changes, with a 30-day ceiling;
on GitHub.com, which has no version and deploys continuously, 14 days. A
changed version, API version, profile revision or classifier revision
forgets the entry. Every later native success at an off-hunk line renews a
`supported` entry. The bound is one entry per operation per repository per
profile, capped like the existing capability caches.

### How it is learned

Only a single comment's Publish is ever a probe. A review never is, because
GitHub refuses a review whole and names no comment (BPR-1735, #124 and #125).

| Placement (P) | Fact | Single Publish | Line in a review (Start a review, Finish your review) |
|---|---|---|---|
| within 3 lines | any | native, as today | native, as today |
| off-hunk | `supported` | native | native |
| off-hunk | `unsupported` | file comment with permalink, no native attempt (RFC-BPR-0022 invariant 2, "first-attempt admission with proven off-hunk placement and fresh, exactly scoped unsupported evidence") | held, with Publish on its own (P) |
| off-hunk | `unknown` | one native attempt, then as below | held, with Publish on its own (P) |
| `unknown` (no merge base) | any | native, as today | as today |

After the one native attempt for an `unknown` key:

- **Created and exactly parsed, at that line:** the comment is published
  natively; record `supported`.
- **Fully received 422 whose error names the line** (field `line` or
  `pull_request_review_thread.line`): record `unsupported`. If the refusal
  matches the server contract's recorded classifier exactly, RFC-BPR-0022's
  file child follows automatically. If not (GHES 3.20's exact tuple is not
  yet recorded), the native child retires as rejected (BPR-1735 AC3), the
  draft is kept, and its row offers Publish as file comment. Because the
  fact is now `unsupported`, the next off-hunk comment on that server goes
  straight to a file comment.
- **Anything else** (timeout, lost or malformed answer, 5xx, rate limit,
  permission, an unrelated 422): no evidence, and RFC-BPR-0022's GET-only
  reconciliation as today.

A refusal is safe to generalize even when it was misread: the cost is honest
file comments until the entry expires. Only a created comment promotes.

### Reviews and BPR-1715's whole-review refusal

A review carries an off-hunk comment natively only when the fact is
`supported`. Evidence from the single-comment operation counts for the
review operation too (decision D3): on GitHub.com both refuse through the
same `pull_request_review_thread.line` check.

If such a review is refused with `refusal=unplaceable_line`, BPR-1715 settles
it as before: it checks the review list, finds no review of mine, and
publishes nothing. The refusal then demotes the fact to `unsupported` for
both operations of that key, the page names the comments P places off-hunk
(BPR-1735 AC4), and those rows are held. The reviewer submits again; that is
a new attempt they start, never an automatic retry. The cost is one wasted
submit, and only if GitHub withdraws support.

### Duplicate-publication safety

Nothing new can publish twice:

- The native attempt is RFC-BPR-0022's native child. A file child follows
  only after a fully received refusal, and never while the native outcome is
  in doubt (invariants 1 to 4).
- A direct file comment is invariant 2's first-attempt admission. It has no
  native child to race.
- The cache never authorizes a resumed publication. After a restart the
  durable publication record decides, as RFC-BPR-0022 already requires.
- A review never mixes a probe with other comments. Its whole-review refusal
  is settled by BPR-1715 before anything else is sent.
- A wrong `supported` costs a refused POST or a refused review, both
  definite non-creations. A wrong `unsupported` costs an honest file comment.

### Where it sits

**Core.** P's rule becomes a three-way placement:

```text
placement(line, fact) =
    in_change_context(line)   -> native
    fact == supported         -> native_off_hunk
    fact == unsupported       -> off_hunk_file
    otherwise                 -> off_hunk_unknown
```

`fact` is an explicit, bounded input in the publication observation; the core
never reads the cache, so equal inputs give equal placements. For a review,
`off_hunk_file` and `off_hunk_unknown` are both held. `plan_review_publication`
gains the table above. `line_in_change_context` and
`github_comment_context_lines` stay as they are (`filtered_comment.hpp`).

**Application host.** Looks the fact up for the exact key before planning,
passes it in, and records evidence from the typed result. It owns the durable
store, its lifetime and the version comparison.

**GitHub adapter.** Reads `installed_version` in the capability probe; keeps
`X-GitHub-Enterprise-Version` and `X-GitHub-Api-Version-Selected` on the
mutation answer; emits typed acceptance or refusal evidence. It never decides
placement.

**Qt.** Shows the marks below. It owns no capability boolean.

### What the UI says

The comment gesture, editor and Publish stay the same on every line.

- **Off-hunk, `supported`:** no mark. Support details on the published thread
  say: "GitHub accepted a comment outside the diff on this server."
- **Off-hunk, `unsupported`, before Publish:** "GitHub can't place a comment
  on this line. It will be posted as a file comment that links to the line."
- **Off-hunk, `unknown`, before Publish:** "GitHub may not place a comment
  on this line. If it refuses, the comment is posted as a file comment that
  links to the line."
- **Held in a review:** P's wording, "GitHub can't place this line in a
  review", with Publish on its own.
- **After a refusal BetterPullReview could not classify:** "GitHub refused a
  line comment here. Publish as file comment?" with the draft kept.
- **Server card, Support details:** "Comments outside the diff: accepted
  (learned 22 Oct from owner/repo, GitHub Enterprise Server 3.23.1)",
  "not accepted (learned …)" or "not known yet". It never says "supported by
  this version".

Implementation of these surfaces loads `frontend-design:frontend-design`
first, as AGENTS.md requires.

## Specification amendments needed

1. **`docs/ux/workbench-specification.md`, "Source-line comments outside a
   hunk".** Replace "Production fallback remains unavailable until an exact
   unsupported response contract is admitted; the UI does not infer support
   from host name or version." with:

   > A line more than three lines from a change is commented natively only
   > when GitHub has accepted such a comment for this profile, repository,
   > server version and operation. Without that, a single comment is posted
   > as a file comment that links to the line, after at most one native
   > attempt for an unknown server, and a review holds it. Support is learned
   > only from GitHub's own answer to a comment, never from the host's name,
   > product or version; a new server version forgets it.

2. **RFC-BPR-0022, a new owner amendment r5** (the canonical criteria stay
   intact, as r2 to r4 did). "Write capability" gains the generalization
   contract the RFC already anticipates ("A repository-wide capability fact
   may generalize support only when the evidence contract explicitly permits
   it"): an observed acceptance or refusal at an anchor the core placed
   off-hunk under P generalizes to that profile, account, repository, server
   version, API version, operation and classifier revision, durable for at
   most 30 days on GHES (ended by a version change) and 14 on GitHub.com. AC8
   is read under r5: version is an invalidation key and part of the
   evidence's scope, never a source of it. The non-goal "Inferring GitHub
   capability from hostname, marketing name, a broad GHES version threshold,
   or the ability to read an existing off-hunk comment" stands unchanged.

3. **BPR-1716 AC3.** Replace it with: "The offer rests on the core's
   placement and this server's learned off-hunk fact, never on the host's
   name or version."

## Decisions for the owner

Each is one Accept or Reject.

- **D1.** Let a learned fact, from GitHub's answer to one of the reviewer's
  own comments, decide native placement beyond three lines for that profile,
  repository and server version (amendment r5). Reject keeps P alone: off-hunk
  lines are always file comments or held, and the application never adopts
  API support when GitHub ships it.
- **D2.** Recorded negatives by version: let the application skip the first
  native attempt on GHES versions up to 3.22 and on GitHub.com, as recorded
  here. Recommended: Reject. One refused POST per repository per server
  version (or per 14 days on GitHub.com) is cheap, and a recorded negative
  would need an application update to lift.
- **D3.** Let single-comment evidence place off-hunk lines in reviews too.
  Reject means reviews keep holding off-hunk lines until a review-operation
  probe exists, which a whole-review refusal makes unsafe to run.
- **D4.** Admit the measured GitHub.com refusal tuple (resource
  `PullRequestReviewComment`, field `pull_request_review_thread.line`, code
  `custom`, message "could not be resolved", any `documentation_url`, any
  header set) as the GitHub.com classifier, so its file comment follows
  automatically. GHES's tuple needs one export after a single Publish of an
  off-hunk comment on the owner's 3.20.8 host; until then a GHES refusal
  records `unsupported` and asks once.

## Proposed acceptance

- AC1: With no learned fact, an off-hunk single Publish makes at most one
  native attempt per key; GitHub's refusal records the fact as unsupported,
  and the comment is published once, as a file comment (automatically, or
  after one confirmation when the refusal is not recognized).
- AC2: With the fact unsupported, an off-hunk comment is a file comment with
  no native attempt, and is held in a review.
- AC3: With the fact supported, an off-hunk comment is a native line comment,
  alone or in a review; a review then refused with an unplaceable line
  publishes nothing, demotes the fact, and holds those comments.
- AC4: A changed `installed_version`, API version, profile revision or
  classifier revision, or expiry, makes the fact unknown. No fact is ever
  set from a host name, product, version, schema or readable comment.
- AC5: No sequence of refusals, timeouts, restarts or demotions publishes a
  comment twice (RFC-BPR-0022's interruption courses, extended with the
  learned path).
- AC6: Support details and the server card say what was learned, where and
  when, in the words above.

## Not verified

- A native off-hunk acceptance has never been observed anywhere, so the
  `supported` path rests on fixtures until GitHub ships API support.
- GHES 3.20.8's exact refusal tuple for a single comment (see D4).
- How GitHub returns a web-made comment on an unchanged line through the
  REST and GraphQL reads (the changelog says existing APIs return them). This
  matters today, independent of this card, because GitHub.com users already
  make such comments in the web page. Checking it needs one such comment made
  in GitHub's web page on BPR-Test, which an agent can't make.

## Owner decisions, 2026-10-22

The owner accepted every recommendation in chat ("I accept all 4 of your recommendations"):

- D1: **Accept.** A learned fact may decide native placement beyond three lines.
- D2: **Reject**, as recommended. The first native attempt is not skipped on versions already known to refuse.
- D3: **Accept.** Single-comment evidence counts for reviews too.
- D4: **Accept.** The measured GitHub.com refusal shape is the general classifier. The GHES 3.20.8 shape still needs one export from the owner after a single off-hunk Publish.
