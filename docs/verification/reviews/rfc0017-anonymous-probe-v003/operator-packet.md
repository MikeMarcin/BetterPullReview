# RFC-BPR-0017 anonymous GraphQL-refusal recapture operator packet, third issue

Status: **pending operator decision; no response recorded; no request issued**

This packet requests one bounded decision: whether the committed RFC-0017
recorder may issue exactly one credential-free, read-only request to
`https://api.github.com/graphql` and persist only the sanitized refusal
contract. The approved course-selection block below is the execution schedule:
one `graphql-refusal` course, expected status 403.

It authorizes nothing else. No credential, mutation, private repository,
disposable repository, GitHub Actions run, GHES instance, proxy, redirect,
retry, quota exhaustion, or rate-limit evasion is permitted. This is not the
separate anonymous-preview product decision, and it does not close
[[BPR-0298]], [[BPR-0445]], or RFC-BPR-0017.

## Why a person is required

The unattended repair is already committed and hermetically verified. A person
must decide whether this repository may point that tooling at GitHub's live
public service once and check the resulting minimized contract into the
repository. The recorder cannot run capture mode without an exact response
bound to this packet's SHA-256.

## Bound authority identity

The authority block is intentionally byte-equivalent, after canonical JSON
serialization, to the consumed v001 authority. Its five targets preserve the
single authority identity shared by the four immutable v001 captures; the
block is an identity catalog, not this packet's execution schedule. The
packet-bound course-selection block that follows selects only
`graphql-refusal` at status 403. Capture mode refuses if the course manifest
differs in target, status, order, or count.

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

## Approved course selection

This is the exact second JSON object capture mode parses. It is part of the
packet digest, and its parsed `courses` array must match the manifest's
`courses` array exactly in target, status, order, and count before any output
reservation, attempt receipt, or request.

```json
{
  "schema_name": "bpr.rfc0017.anonymous_course_selection",
  "schema_version": 1,
  "courses": [
    {
      "name": "graphql-refusal",
      "expected_status": 403
    }
  ]
}
```

## What the one course records

The request is a credential-free POST to GitHub's GraphQL endpoint with no
query body. The previously observed contract was a 403 refusal with the
`graphql` rate-limit resource and limit, remaining, and used values all zero.
Its message embedded the caller's address, so that first live body was deleted
uncommitted.

The repaired operation accepts only that narrow 403 zero-quota shape with
exactly one valid IPv4 or IPv6 caller address. Before persistence it replaces
the address with `[normalized-caller-address]`, records
`caller_address_normalized: true`, and hashes the normalized body. Any other
status, quota shape, message shape, duplicate member, missing/malformed/
multiple address, or residual address is a refusal. The raw response body is
never written.

One request is the entire authorized execution. GitHub may have changed the
live refusal since v001; if the shape is different, the recorder refuses after
consuming this packet's single issuance. A new packet and decision would be
required to try again.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `scripts/record_rfc0017_anonymous_contracts.py` | `sha256:c438018780961f1bacead772d33d50b65fcc31ca713b1f609f531de6268a9302` | 39854 |
| `tests/scripts/record_rfc0017_anonymous_contracts_tests.py` | `sha256:a7f2711fe01243106bf35723b85fd641a18f2dbc4c1229914998d1d490595868` | 43972 |

The sanitizer repair is committed at
`a27e4a5f3a6effd5123300a32c60caff07b682e7`; the exact course-selection
binding is committed at `c03abcf2d3af25d4b74dc231f95b44c83522cdbf`.
The focused suite passes 82/82, `py_compile` passes, and the immutable v001
evidence still validates as `RFC0017_ANONYMOUS_CONTRACTS_VALID:4`. Independent
capability-critical review reports no blocking finding or verification failure.
No network request ran while preparing or verifying this packet.

## Mechanical pre-request bounds

Capture mode refuses before traffic unless all of these remain exact:

- the response carries this packet's one SHA-256 and one exact quoted approval
  using the recorder's canonical response byte grammar;
- the current recorder digest and byte count match the bound-evidence row;
- every recognized lowercase `json` fenced object block parses without
  duplicate members, and the packet has exactly one authority and one
  course-selection schema object regardless of member order;
- the manifest binds this packet and exactly matches the one selected course;
- the output is a safe descendant of this packet directory and the destination
  does not exist;
- a repository-wide packet-identity attempt receipt is exclusively created,
  flushed, and durably synced before the request;
- the request remains an HTTPS POST to `api.github.com/graphql` with no ambient
  proxy and no redirect.

Within this checkout, the recorder never deletes an attempt receipt, even if
GitHub is unavailable or the returned shape is refused. Copying the packet
directory or choosing another contained output path in this checkout does not
create another issuance. Deliberate receipt deletion or duplication of the
whole checkout is outside this local mechanism; BPR-0298's pending immutable-
evidence authentication owns that repository-external boundary.

## Supersedes v002

The v002 packet (`8ff060e82c04f30010ad11e5410200d8797d8f1c5b08557768ded86ed0eafdd0`)
was approved in the 2026-08-27 attended session and its capture attempt ran
on the Windows host, where the recorder's durability barrier refused after
durably reserving the attempt receipt but strictly before any request: the
attempt registry directory cannot be opened for durable sync on Windows.
Zero requests ran; the receipt consumed that packet's single issuance under
the never-delete rule. v002, its response, and its receipt stay byte-frozen
as that record. This packet is the successor; it changes no authority,
course, bound, or recorder byte. Its capture must run from a POSIX-capable
surface against this checkout — WSL over `/mnt/c` is proven to satisfy the
directory-sync barrier here.

## Operator response

Return exactly one of these decisions in the attended session.

Approval:

> I authorize exactly the credential-free read-only requests named below against api.github.com.

Rejection:

> I reject this anonymous public-read probe.

An approval permits the recorder to execute the packet-bound one-course
selection once. Packet preparation, commentary, paraphrase, or a partial
response is not approval. On approval, the integrator will record only the
canonical digest-bound response bytes, validate the committed packet/manifest,
and then run the one selected course. On rejection, no response is used for
capture and no request runs.
