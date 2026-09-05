# External authority

Captured from official GitHub sources on 2026-09-04. These facts motivate the
RFC; live operation behavior still requires exact recorded evidence.

## GitHub.com Files changed behavior

GitHub announced on 2025-09-25 that the new Files changed page could comment on
any line of a changed file. The announcement states:

- the feature rolled out gradually per repository;
- comments can be added only to files already changed by the pull request;
- API support was limited, while existing APIs and webhooks would return the
  comments.

Source:
<https://github.blog/changelog/2025-09-25-pull-request-files-changed-public-preview-now-supports-commenting-on-unchanged-lines/>

Design consequence: host name or readable examples do not prove the exact write
operation for this repository.

## Public REST review-comment contract

The create-review-comment endpoint describes itself as creating a comment on
the pull-request diff. Its body parameters state:

- `line` is required unless using `subject_type:file`;
- `subject_type` is `line` or `file`;
- `422` means validation failed or the endpoint has been spammed.

GitHub.com source:
<https://docs.github.com/en/rest/pulls/comments#create-a-review-comment-for-a-pull-request>

GHES 3.18 source:
<https://docs.github.com/en/enterprise-server@3.18/rest/pulls/comments#create-a-review-comment-for-a-pull-request>

Design consequences:

- a file-level review comment is an ordinary documented fallback primitive;
- status 422 alone cannot mean off-hunk capability is unsupported;
- request/response evidence must be operation- and contract-specific.

## GHES 3.21 errata

The 3.21 release notes state that an earlier note claiming reviewers could
comment on any line of a changed file was added prematurely and removed, and
would be republished when ready. The erratum is dated 2026-06-23.

Source:
<https://docs.github.com/en/enterprise-server@3.21/admin/release-notes>

Design consequence: no broad GHES version threshold is trustworthy without an
exact operation contract.

## Permanent code links

GitHub documents permanent links to a specific line or range in a specific
version of a file. A same-repository permalink renders as a code snippet in a
comment. Markdown source uses `?plain=1` before the line fragment.

Source:
<https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-a-permanent-link-to-a-code-snippet>

Design consequence: the visible fallback can preserve exact code context for a
GitHub-only collaborator without hidden BetterPullReview metadata.

## What remains unproven

- Whether the current BetterPullReview REST operation can create an off-hunk
  line comment on GitHub.com today.
- The exact successful response shape if it can.
- The exact structured unsupported error for each legacy GHES contract.
- Whether file-subject review comments expose identical reply/resolution UI on
  every target host.

The designated-live issue owns these observations. The RFC treats them as
unknown until recorded.
