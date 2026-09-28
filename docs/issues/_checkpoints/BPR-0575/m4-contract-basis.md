# RFC-0022 bounded rejection contract basis

Assignment `/root/rfc0022_m4`, mandate r2. Production candidate `699c921c`.
This is supporting evidence for an owner decision, not an approved classifier.

## Operation and eligibility

The shipped operation is `create_review_comment`, REST
`POST /repos/MikeMarcin/BPR-Test/pulls/2/comments`, GitHub.com server contract,
`X-GitHub-Api-Version: 2022-11-28`, native RIGHT source-line representation.
The exact signed application admits the accepted source, current head, changed
file, materialized line and authenticated MikeMarcin identity before its write.
Each capsule and one-use approval has a separate immutable identity record.

[Provider eligibility](m4-live-anchor-eligibility.json) records exact repository,
PR, head and base identities, verified Git blob identities, 80-line source
contents and the complete small provider patches. Both files are modified;
each patch changes one line and covers RIGHT7–13. RIGHT61 exists outside the
patch. This establishes an eligible off-hunk source anchor independently of
Qt geometry. It does not establish the service's reason for rejecting a write.

## Authoritative public documentation

GitHub's [create-review-comment contract](https://docs.github.com/en/rest/pulls/comments#create-a-review-comment-for-a-pull-request)
describes `line` in the pull-request diff, permits the file subject, and lists
creation separately from validation/spam refusal. The documentation does not
promise that every422 is a definitive off-hunk rejection or define one exact
unsupported tuple for this case.

GitHub's [validation troubleshooting](https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api#validation-failed)
explains that a validation failure can indicate an unprocessed request, and
that the `custom` code requires consulting its message. Thus neither the status
nor `PullRequestReviewComment`/`custom` alone determines the rejection reason.
An exact message dependency needs the RFC's explicit owner acceptance, with
the recorded complete shape and eligibility proof. No broad meaning for a
resolution error is presumed.

These pages were inspected during this run. Their current examples use a newer
API header; our actual request remains pinned to2022-11-28. General public
explanations are supporting evidence, not a substitute for the observed
operation/version-specific response.

## Durable safety obligations

TXT60, Markdown60, TXT61 and Markdown61 remain unresolved native publications with no
fallback authority. Their prior records and one-use approvals remain intact.
An owner decision about a newly captured contract does not retroactively supply
a missing response to any earlier attempt. Empty GET results are not proof of
non-creation. Future code must refuse every mismatch, stale eligibility,
unknown member, different message or ambiguous transport outcome.

## Concurrent tracker maintenance

Documentation replay detected historical counter BPR-1221 from concurrent
commit `36d535eb` while this checkout still held BPR-1220. The counter was
synchronized to that observed high water without allocating an ID or creating
a reservation. That owning commit exposes no corresponding1220receipt; none
is invented here. Only the historical counter is synchronized. The earlier
passing gate remains valid for its earlier history.

## Complete final observation

The [Markdown61 attempt](m4-resolution-live-attempt.json) captured the exact
`PullRequestReviewComment` / `pull_request_review_thread.line` / `custom` /
`could not be resolved` tuple, with canonical documentation/status metadata,
one error and zero unknown members. The [owner proposal](m4-owner-contract.md)
binds the actual shape and eligibility proof to a prospective classifier
revision 2. Revision 1 refuses the observed extra metadata, and production
classification remains disabled. The proposal records the weaker message
meaning and the need for fresh runtime header checks; no historical journal is
reclassified.
