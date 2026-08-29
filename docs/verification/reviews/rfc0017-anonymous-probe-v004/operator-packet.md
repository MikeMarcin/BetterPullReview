# RFC-BPR-0017 anonymous conditional-304 accounting operator packet, first issue

Status: **pending operator decision; no response recorded; no request issued**

This packet requests one bounded decision: whether the committed RFC-0017
recorder may issue exactly one credential-free, read-only conditional GET to
`https://api.github.com/repos/octocat/Hello-World/pulls/1`, replaying the
already-committed v001 `pull-request` capture's exact `ETag` and
`Last-Modified` as `If-None-Match` and `If-Modified-Since`, and persist only
the sanitized `304 Not Modified` contract. The approved course-selection
block below is the execution schedule: one `pull-request-conditional`
course, expected status 304.

It authorizes nothing else. No credential, mutation, private repository,
disposable repository, GitHub Actions run, GHES instance, proxy, redirect,
retry, quota exhaustion, or rate-limit evasion is permitted. This is not the
separate anonymous-preview product decision, and an approval does not by
itself close [[BPR-0298]] or RFC-BPR-0017.

## Why a person is required

The unattended mechanism is committed and hermetically verified — 151 tests,
a 17/17 mutation sweep over the new logic, and the seeded v001 validation
byte-identical. A person must decide whether this repository may point that
tooling at GitHub's live public service once more and check the resulting
minimized contract into the repository. The recorder cannot run capture mode
without an exact response bound to this packet's SHA-256.

## Bound authority identity

This authority is new, at schema version 2: the conditional operation cannot
be named by a version-1 authority at all, because its target must pin the
exact committed capture whose validators it replays. This packet's capture
is therefore written under this packet's own directory and bound to this
authority's digest; the immutable v001 authority, captures, and bindings are
untouched by it.

```json
{
  "schema_name": "bpr.rfc0017.anonymous_probe_authority",
  "schema_version": 2,
  "statement": "I authorize exactly the credential-free read-only conditional request named below against api.github.com.",
  "targets": [
    {
      "name": "pull-request-conditional",
      "operation": "rest_conditional_not_modified",
      "url": "https://api.github.com/repos/octocat/Hello-World/pulls/1",
      "condition_source": {
        "capture_file": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request.json",
        "file_sha256": "0f0737d8572f16cb77cd6a7d6c8734cd69adf15d37de73567e83d7b33316445f"
      }
    }
  ]
}
```

## Approved course selection

This is the exact second JSON object capture mode parses. It is part of the
packet digest, and its parsed `courses` array must match the manifest's
`courses` array exactly in target, status, order, and count before any
output reservation, attempt receipt, or request.

```json
{
  "schema_name": "bpr.rfc0017.anonymous_course_selection",
  "schema_version": 1,
  "courses": [
    {
      "name": "pull-request-conditional",
      "expected_status": 304
    }
  ]
}
```

## What the one course records

The request is a credential-free GET carrying exactly two request headers,
both built from the pinned committed capture after its bytes are verified
against the digest above: `If-None-Match` from its recorded `etag`
(`b651e01dc2085840542ae4c9d01c50a611531327abac78af83330e807d53bf6f`) and
`If-Modified-Since` from its recorded `last-modified`
(`Sun, 23 Aug 2026 03:57:54 GMT`). No other header is possible: the
transport accepts only those two names, and only for this operation.

A `304 Not Modified` answer has an empty body by contract; the recorder
refuses any non-empty body and persists `body: null` with the empty-bytes
digest plus the allowlisted response headers. Those retained headers are the
course's whole point: the fresh `etag` beside the request's, and the
`x-ratelimit-limit`/`remaining`/`used` counters that witness whether an
anonymous conditional 304 consumes the 60-per-hour core budget — the
"conditional 304 accounting" fact RFC-BPR-0017's ledger needs and no
committed capture yet records.

One request is the entire authorized execution, and two live outcomes end
without a committed capture, stated plainly:

- If `octocat/Hello-World#1` changed since the pinned `ETag` was recorded,
  the answer is `200` with a body. The recorder refuses on the non-empty
  body **after** the single-use attempt receipt exists, so this packet's one
  issuance is consumed with nothing committed, exactly as the graphql
  refusal precedent worked. A fresh packet and decision would be required.
- Any other status, a body on the 304, or header-shape drift refuses the
  same way.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `scripts/record_rfc0017_anonymous_contracts.py` | `sha256:ebbbf370928d9cb39549724410ba675dd50770fcb9ad22fb590a168900a69f72` | 76007 |
| `tests/scripts/record_rfc0017_anonymous_contracts_tests.py` | `sha256:35a781df8bf07eaac649cd01bc4dace507df958f147198f527dc6184cf8cfa1d` | 100069 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request.json` | `sha256:0f0737d8572f16cb77cd6a7d6c8734cd69adf15d37de73567e83d7b33316445f` | 17077 |

The conditional mechanism is committed at
`f7ba12e7` and the external capture binding at `4d166772`; the pinned
condition source's owning commit is `4d250ab63e544d903435a10ae04e3ded8f96cf8b`
per `capture-bindings.json`. The focused suite passes 151/151, the seeded
v001 validation prints `RFC0017_ANONYMOUS_CONTRACTS_VALID:5`, `BOUND:5`,
and `DERIVED:1`, and no network request ran while preparing or verifying
this packet.

## Mechanical pre-request bounds

Capture mode refuses before traffic unless all of these remain exact:

- the response carries this packet's one SHA-256 and one exact quoted
  approval using the recorder's canonical response byte grammar;
- the current recorder digest and byte count match the bound-evidence row;
- every recognized lowercase `json` fenced object block parses without
  duplicate members, and the packet has exactly one authority and one
  course-selection schema object regardless of member order;
- the manifest binds this packet and exactly matches the one selected
  course;
- the pinned condition source resolves inside the repository, matches its
  digest byte-for-byte, and records an `etag` — proven **before** the
  single-use attempt receipt is reserved, so an unprovable source cannot
  consume the issuance;
- the output is a safe descendant of this packet directory and the
  destination does not exist;
- a repository-wide packet-identity attempt receipt is exclusively created,
  flushed, and durably synced before the request;
- the request remains an HTTPS GET to the one URL above with no ambient
  proxy and no redirect.

Within this checkout, the recorder never deletes an attempt receipt, even
if GitHub is unavailable or the returned shape is refused.
`capture-bindings.json` and its owning-commit verification own the
repository-external evidence boundary.

## Operator response

Return exactly one of these decisions in the attended session.

Approval:

> I authorize exactly the credential-free read-only conditional request named below against api.github.com.

Rejection:

> I reject this anonymous conditional probe.

An approval permits the recorder to execute the packet-bound one-course
selection once. Packet preparation, commentary, paraphrase, or a partial
response is not approval. On approval, the integrator will record only the
canonical digest-bound response bytes, validate the committed
packet/manifest, and then run the one selected course. On rejection, no
response is used for capture and no request runs.
