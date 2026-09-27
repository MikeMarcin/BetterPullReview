# Proposed owner decision: exact off-hunk rejection contract

Observed candidate `699c921c8a0e804f57ab1b806e3c0b4bf685fb8a`, mandate r2.
Production classification is disabled. This decision is pending.

Accept the exact recorded contract below as a bounded dependency for a new
classifier revision 2, limited to the designated PR, accepted head and verified
off-hunk source anchors, while preserving all transport, identity and recovery
refusals?

## Actual observation

One ordinary Publish from the signed production capsule, as MikeMarcin in
profile 2, selected `off_hunk_fixture.md`, RIGHT61, at head
`61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a` on
https://github.com/MikeMarcin/BPR-Test/pull/2. The independent comment body was
“BetterPullReview Markdown source-line verification at line 61.” The authority
was fresh at 06:06 of its 15-minute lifetime. No retry or fallback occurred.

Operation: `create_review_comment`, native RIGHT line subject, REST
`POST /repos/MikeMarcin/BPR-Test/pulls/2/comments`, GitHub.com server contract,
API `2022-11-28`. The complete bounded HTTP 422 body had these exact typed values:

```json
{
  "message": "Validation Failed",
  "documentation_url": "https://docs.github.com/rest/pulls/comments#create-a-review-comment-for-a-pull-request",
  "status": "422",
  "errors": [{
    "resource": "PullRequestReviewComment",
    "field": "pull_request_review_thread.line",
    "code": "custom",
    "message": "could not be resolved"
  }]
}
```

There was exactly one error and zero unknown envelope or error members. All
listed values are strings; the error message has no period. This is a typed
reconstruction of closed diagnostic fields, not a raw-response log. The current
classifier is revision 1 and refuses this complete envelope. No production
contract is installed.

## Eligibility and limits of the evidence

[Independent provider evidence](m4-live-anchor-eligibility.json) binds repository
1328277631, PR4650736621, base `a939a85a5525994fe9e9cb4c973ad9362ed427c7`, the
accepted head above, and Markdown blob `828d5c87c20a0f9d74c498e04c36b32d2948b7c5`.
The verified file has 80 lines, including the exact selected line61. Its complete
provider patch is `@@ -7,7 +7,7 @@`, with one addition and one deletion matching
the provider totals. Thus RIGHT61 exists in a changed file and is outside the
provider hunk. This proof is independent of Qt geometry.

GitHub's [operation documentation](https://docs.github.com/en/rest/pulls/comments#create-a-review-comment-for-a-pull-request)
describes line placement in the diff. Its
[validation documentation](https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api#validation-failed)
explains unprocessed validation failures and says that a custom code requires
its message. Neither page declares this exact tuple an off-hunk discriminator.
The message “could not be resolved” can describe other validation failures.
No stable off-hunk-specific code was supplied in this observed complete shape.
Accept therefore explicitly accepts the bounded message dependency and its
remaining interpretation risk; it does not invent a GitHub guarantee.

## Proposed implementation boundary

The next engineering round must implement and verify revision 2 before any
activation. Its recorded-contract scope must bind this GitHub.com repository,
PR, accepted head, MikeMarcin actor and profile2, the exact operation/API revision,
and a changed file whose exact blob and selected line are revalidated. An anchor
must be proven outside its complete bounded provider patch. This is not a
blanket GitHub.com or GHES capability claim. Other repositories, heads, actors,
profiles, API revisions, operations and shapes remain unknown.

Match every observed member, count, type and literal above. Retain complete
bounded transport response and valid metadata requirements. Refuse Retry-After,
Location, invalid metadata, unknown/duplicate members, other values or messages,
extra errors, stale eligibility, permission/rate-limit outcomes and ambiguous
transport. The observation does not attest Retry-After absence; future runtime
classification must check it. Negative mismatch and public-path tests must pass.

All four earlier possibly transmitted publications stay GET-only with no
fallback authorization. The accepted contract cannot supply missing response
metadata retroactively. Empty GET results do not prove non-creation. A future
live fallback course needs a genuinely separate approved logical comment and a
fresh exact one-use application approval.

Accept authorizes this narrow contract dependency for implementation; it does
not mark live fallback, native success, collaborator continuity or RFC closeout
as passed. Reject keeps production classification disabled and requires a
stronger exact contract or a revised proposal. Put the requested change in the
note. Full observation: [MD61 attempt](m4-resolution-live-attempt.json).
