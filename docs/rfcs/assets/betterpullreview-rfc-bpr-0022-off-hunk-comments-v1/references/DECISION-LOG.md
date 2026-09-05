# Decision log

## D1 — New RFC, not an edit to RFC-BPR-0008

RFC-BPR-0008 owns the general round-trip mutation contract. The new behavior is
large enough to add capability negotiation, a second wire representation,
recovery migration, normalization, and UX criteria, so it is RFC-BPR-0022 and
depends on RFC-BPR-0008.

## D2 — Semantic anchor is the product abstraction

The reviewer acts on an exact source line. GitHub's line/file subject is a
provider representation. BetterPullReview retains both instead of allowing a
file-subject fallback to erase the selected line or pretending GitHub stored a
line thread.

## D3 — File-level review comment is the fallback

A changed file already has an ordinary GitHub review-comment primitive with
`subject_type:file`. The fallback uses it, not a PR timeline comment and not a
comment attached to a nearby changed line.

## D4 — The visible permalink is the recovery grammar

The fallback body starts with one same-repository full-commit code permalink,
then a blank line, then byte-preserved reviewer prose. There is no hidden HTML
comment, marker, label, bot, or opaque ID. This keeps GitHub-only and database-
loss behavior honest.

## D5 — The actual publication is the probe

No disposable probe comment is created. For unknown support, the first user-
approved off-hunk publication attempts the intended native operation. Success
records support; an exact definitive rejection may authorize fallback.

## D6 — Generic failure never means unsupported

Status 422 is ambiguous by GitHub's own contract. Permission, rate limit,
network, cancellation, malformed, and 5xx outcomes also cannot authorize
fallback. The classifier is exact and revisioned.

## D7 — No fallback after an indeterminate native write

A native comment that may exist and a fallback comment would duplicate one
logical reviewer action. Therefore no state reachable from
`native_possibly_transmitted` can create a fallback attempt. Recovery is GET-
only until the native result is authoritative.

## D8 — Parent publication plus deterministic child attempts

Native and fallback requests have different bodies and anchors. They are child
attempts of one logical publication, each with its own fingerprint, baseline,
and transmission state. Reusing one wire idempotency record is prohibited.

## D9 — Repository-scoped capability evidence

The GitHub.com rollout was per repository. Evidence is also scoped by actor,
profile, server/API contract, operation, and classifier revision. Existing read
capability cache entries are not automatically repurposed.

## D10 — Provider placement is not Qt geometry

BetterPullReview's full-file diff and rendered hunk grouping are its own
semantic/presentation artifacts. They cannot authoritatively state what a
GitHub API will accept. Unknown placement negotiates by the real operation.

## D11 — Invisible in normal UX, honest in diagnostics

The line affordance/editor/publish/result flow is identical. A support detail
may say that a file comment with linked code was used. A semantic loss such as
non-applyable suggestion behavior is not hidden and receives a typed refusal or
explicit limitation.

## D12 — Changed files and current/right single lines only

This vertical slice matches the existing mutation binding and GitHub's announced
comment-anywhere scope. Entirely unchanged files, left-side/historical anchors,
and ranges remain separate work.

## D13 — RFC-BPR-0021 remains separate

A fallback URL created by BetterPullReview carries an exact current-head commit.
That does not solve revision identity for arbitrary native/external historical
comments. RFC-BPR-0021 retains ownership of that general join.
