# RFC-BPR-0017 anonymous public-read probe operator packet

Status: **pending operator decision; not RFC closeout, not a build decision, not repository-mutation or credential approval**

This packet requests one bounded decision: whether
`scripts/record_rfc0017_anonymous_contracts.py` may issue exactly the
credential-free, read-only requests named in the authority block below against
`api.github.com`, and record their sanitized responses.

It authorizes nothing else. No credential, no mutation, no private repository,
no disposable repository creation or deletion, no GitHub Actions, no GHES
instance, and no retry past a rate limit. It does not decide whether the
anonymous public preview is built — that is a separate packet — and it does not
close [[BPR-0298]] or RFC-BPR-0017.

## Why a person is required

The requests are harmless individually. What needs a person is the decision to
point this repository's tooling at a live third-party service at all, and to
accept that the responses recorded from it become checked-in evidence. The
script refuses to issue a single request until it has read an approval carrying
the statement below byte-for-byte.

## The bound authority

This is the exact block the script parses. Its digest is what a capture is
recorded under, so editing any character of it — including choosing a different
repository or pull request — invalidates every capture already taken and is the
intended way to change what is authorized.

```json
{
  "schema_name": "bpr.rfc0017.anonymous_probe_authority",
  "schema_version": 1,
  "statement": "I authorize exactly the credential-free read-only requests named below against api.github.com.",
  "targets": [
    {
      "name": "rate-limit",
      "operation": "rest_rate_limit",
      "url": "https://api.github.com/rate_limit"
    },
    {
      "name": "graphql-refusal",
      "operation": "graphql_unauthenticated_refusal",
      "url": "https://api.github.com/graphql"
    },
    {
      "name": "absent-not-disclosed",
      "operation": "rest_absent_not_disclosed",
      "url": "https://api.github.com/repos/MikeMarcin/bpr-nonexistent-probe-target"
    },
    {
      "name": "pull-request",
      "operation": "rest_pull_request",
      "url": "https://api.github.com/repos/octocat/Hello-World/pulls/1"
    },
    {
      "name": "pull-request-comments",
      "operation": "rest_pull_request_comments",
      "url": "https://api.github.com/repos/octocat/Hello-World/pulls/1/comments?per_page=1"
    }
  ]
}
```

## What each target is for, and what it costs

| target | records | quota cost |
|---|---|---|
| `rate-limit` | the primary and search budget headers, unauthenticated | none; `/rate_limit` is exempt |
| `graphql-refusal` | that GitHub's GraphQL API refuses an unauthenticated caller. This is the contract that decides whether an anonymous preview can show inline review comments at all, and it is expected to be a refusal, not a failure of the probe | 1 request |
| `absent-not-disclosed` | that a missing repository is reported as absent without disclosing whether it exists privately | 1 request |
| `pull-request` | the anonymous REST shape for a pull request | 1 request |
| `pull-request-comments` | the anonymous REST review-comment shape and its paging headers, bounded to one item | 1 request |

Five requests total against a 60-per-hour unauthenticated budget. The course
never exhausts the quota and never attempts to evade it.

## What could not be verified without a request

`octocat/Hello-World` is GitHub's canonical public sample repository, and pull
request `1` is named here because it is the conventional first. **This packet
cannot confirm that pull request is live**, because confirming it would require
exactly the request being authorized. If it is absent the capture records a 404
and the target should be replaced with a public pull request of your choosing —
which changes the authority digest, which is the mechanism working rather than
failing.

Conditional-`304` accounting and synthetic rate exhaustion are deliberately
absent from this authority. Both depend on values only the first capture can
supply: a real `ETag` for the conditional request, and real budget headers to
shape a synthetic exhaustion from. They belong to a later authority version,
and [[BPR-0298]] owns them.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `scripts/record_rfc0017_anonymous_contracts.py` | `sha256:2a383ddbde8f155ed4c5808e635cbe1bad3091a199fde3e4f3a8a18f82dc38a7` | 22021 |
| `tests/scripts/record_rfc0017_anonymous_contracts_tests.py` | `sha256:76ae18118d53a5cb5e2bdee467feb66858c8aee3d3f1287ac6b667a08f547e4f` | 13788 |

The script holds no network client at import time; `urllib` is imported inside
the single function that issues a request. Its `--validate-existing` mode is
proven to reach no network by a test that replaces that function with one that
fails if it is ever called.

## What the script refuses, mechanically

These are outer bounds the authority cannot widen, each covered by a test:

- any host but `api.github.com`, any scheme but `https`, any explicit port, any
  embedded credential, any URL fragment;
- any operation not in its table; `POST` anywhere but `/graphql`, and `GET` at
  `/graphql`;
- an output root that is not a directory beneath this packet's own directory;
- a response header outside a retained allowlist — dropped, not redacted,
  because a dropped header cannot leak;
- any persisted byte matching an authorization header, bearer or basic
  credential, GitHub token of any prefix, private key header, cookie, or
  credential-bearing URL;
- a body that is not JSON, or larger than 1 MiB.

## Operator response

Return one of these exact decisions:

> I authorize exactly the credential-free read-only requests named below against api.github.com.

or:

> I reject this anonymous public-read probe.

An approval authorizes the five requests in the authority block above, once.
Neither packet preparation nor a partial response is an approval.
